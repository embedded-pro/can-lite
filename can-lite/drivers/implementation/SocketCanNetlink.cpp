#ifdef __linux__

#include "can-lite/drivers/implementation/SocketCanNetlink.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <linux/can/netlink.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace services::socketcan
{
    namespace
    {
        constexpr std::size_t Align(std::size_t size)
        {
            return (size + 3u) & ~std::size_t{ 3u };
        }

        constexpr uint16_t AttributeType(uint16_t type)
        {
            return type & NLA_TYPE_MASK;
        }

        class MessageWriter
        {
        public:
            explicit MessageWriter(infra::ByteRange buffer)
                : buffer(buffer)
            {
                std::fill(buffer.begin(), buffer.end(), uint8_t{});
            }

            template<class T>
            void Put(const T& value)
            {
                PutBytes(&value, sizeof(T));
            }

            void PutBytes(const void* data, std::size_t length)
            {
                if (size + Align(length) > buffer.size())
                {
                    valid = false;
                    return;
                }

                std::memcpy(buffer.begin() + size, data, length);
                size += Align(length);
            }

            void PutAttribute(uint16_t type, const void* data, std::size_t length)
            {
                Put(rtattr{ static_cast<unsigned short>(sizeof(rtattr) + length), type });
                PutBytes(data, length);
            }

            std::size_t BeginNest(uint16_t type)
            {
                const auto start = size;
                Put(rtattr{ 0, static_cast<unsigned short>(type | NLA_F_NESTED) });
                return start;
            }

            void EndNest(std::size_t start)
            {
                PatchLength<unsigned short>(start, size - start);
            }

            std::size_t Finish()
            {
                PatchLength<uint32_t>(0, size);
                return valid ? size : 0;
            }

        private:
            template<class Length>
            void PatchLength(std::size_t offset, std::size_t length)
            {
                if (!valid)
                    return;

                const auto value = static_cast<Length>(length);
                std::memcpy(buffer.begin() + offset, &value, sizeof(value));
            }

            infra::ByteRange buffer;
            std::size_t size{};
            bool valid{ true };
        };

        MessageWriter StartLinkMessage(infra::ByteRange buffer, uint16_t type, uint16_t flags, int interfaceIndex, uint32_t sequence, unsigned int linkFlags, unsigned int change)
        {
            MessageWriter writer{ buffer };

            nlmsghdr header{};
            header.nlmsg_type = type;
            header.nlmsg_flags = flags;
            header.nlmsg_seq = sequence;
            writer.Put(header);

            ifinfomsg info{};
            info.ifi_family = AF_UNSPEC;
            info.ifi_index = interfaceIndex;
            info.ifi_flags = linkFlags;
            info.ifi_change = change;
            writer.Put(info);

            return writer;
        }

        template<class Visitor>
        void ForEachAttribute(infra::ConstByteRange attributes, Visitor visitor)
        {
            while (attributes.size() >= sizeof(rtattr))
            {
                rtattr header{};
                std::memcpy(&header, attributes.begin(), sizeof(header));
                if (header.rta_len < sizeof(rtattr) || header.rta_len > attributes.size())
                    return;

                visitor(AttributeType(header.rta_type), infra::ConstByteRange(attributes.begin() + sizeof(rtattr), attributes.begin() + header.rta_len));
                attributes = infra::ConstByteRange(attributes.begin() + std::min<std::size_t>(Align(header.rta_len), attributes.size()), attributes.end());
            }
        }

        bool IsCanKind(infra::ConstByteRange kind)
        {
            static constexpr std::array<uint8_t, 3> can{ 'c', 'a', 'n' };
            const auto length = std::find(kind.begin(), kind.end(), uint8_t{}) - kind.begin();
            return length == static_cast<std::ptrdiff_t>(can.size()) && std::equal(can.begin(), can.end(), kind.begin());
        }

        std::optional<uint32_t> DecodeBitrate(infra::ConstByteRange canData)
        {
            std::optional<uint32_t> bitrate;
            ForEachAttribute(canData, [&bitrate](uint16_t type, infra::ConstByteRange value)
                {
                    if (type != IFLA_CAN_BITTIMING || value.size() < sizeof(can_bittiming))
                        return;

                    can_bittiming timing{};
                    std::memcpy(&timing, value.begin(), sizeof(timing));
                    bitrate = timing.bitrate;
                });
            return bitrate;
        }

        void DecodeLinkInfo(infra::ConstByteRange linkInfo, LinkState& state)
        {
            ForEachAttribute(linkInfo, [&state](uint16_t type, infra::ConstByteRange value)
                {
                    if (type == IFLA_INFO_KIND)
                        state.isCan = IsCanKind(value);
                    else if (type == IFLA_INFO_DATA)
                        state.bitrate = DecodeBitrate(value);
                });
        }

        std::optional<LinkState> DecodeLinkMessage(infra::ConstByteRange payload)
        {
            if (payload.size() < sizeof(ifinfomsg))
                return std::nullopt;

            ifinfomsg info{};
            std::memcpy(&info, payload.begin(), sizeof(info));

            LinkState state;
            state.up = (info.ifi_flags & IFF_UP) != 0;

            ForEachAttribute(infra::ConstByteRange(payload.begin() + std::min(Align(sizeof(ifinfomsg)), payload.size()), payload.end()), [&state](uint16_t type, infra::ConstByteRange value)
                {
                    if (type == IFLA_LINKINFO)
                        DecodeLinkInfo(value, state);
                });

            if (!state.isCan)
                state.bitrate.reset();

            return state;
        }

        template<class Visitor>
        void ForEachMessage(infra::ConstByteRange messages, Visitor visitor)
        {
            while (messages.size() >= sizeof(nlmsghdr))
            {
                nlmsghdr header{};
                std::memcpy(&header, messages.begin(), sizeof(header));
                if (header.nlmsg_len < sizeof(nlmsghdr) || header.nlmsg_len > messages.size())
                    return;

                if (visitor(header.nlmsg_type, infra::ConstByteRange(messages.begin() + sizeof(nlmsghdr), messages.begin() + header.nlmsg_len)))
                    return;

                messages = infra::ConstByteRange(messages.begin() + std::min<std::size_t>(Align(header.nlmsg_len), messages.size()), messages.end());
            }
        }
    }

    std::size_t EncodeGetLink(infra::ByteRange buffer, int interfaceIndex, uint32_t sequence)
    {
        return StartLinkMessage(buffer, RTM_GETLINK, NLM_F_REQUEST, interfaceIndex, sequence, 0, 0).Finish();
    }

    std::size_t EncodeSetLinkUp(infra::ByteRange buffer, int interfaceIndex, bool up, uint32_t sequence)
    {
        return StartLinkMessage(buffer, RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK, interfaceIndex, sequence, up ? IFF_UP : 0u, IFF_UP).Finish();
    }

    std::size_t EncodeSetBitrate(infra::ByteRange buffer, int interfaceIndex, uint32_t bitrate, uint32_t sequence)
    {
        auto writer = StartLinkMessage(buffer, RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK, interfaceIndex, sequence, 0, 0);

        const auto linkInfo = writer.BeginNest(IFLA_LINKINFO);
        static constexpr std::array<char, 3> kind{ 'c', 'a', 'n' };
        writer.PutAttribute(IFLA_INFO_KIND, kind.data(), kind.size());

        const auto data = writer.BeginNest(IFLA_INFO_DATA);
        can_bittiming timing{};
        timing.bitrate = bitrate;
        writer.PutAttribute(IFLA_CAN_BITTIMING, &timing, sizeof(timing));
        writer.EndNest(data);

        writer.EndNest(linkInfo);
        return writer.Finish();
    }

    std::optional<LinkState> DecodeLink(infra::ConstByteRange response)
    {
        std::optional<LinkState> state;
        ForEachMessage(response, [&state](uint16_t type, infra::ConstByteRange payload)
            {
                if (type != RTM_NEWLINK)
                    return false;

                state = DecodeLinkMessage(payload);
                return true;
            });
        return state;
    }

    std::optional<int> DecodeAcknowledgement(infra::ConstByteRange response)
    {
        std::optional<int> error;
        ForEachMessage(response, [&error](uint16_t type, infra::ConstByteRange payload)
            {
                if (type != NLMSG_ERROR || payload.size() < sizeof(int))
                    return false;

                int code{};
                std::memcpy(&code, payload.begin(), sizeof(code));
                error = -code;
                return true;
            });
        return error;
    }

    RouteSocket::RouteSocket()
        : descriptor(::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE))
    {
        if (descriptor < 0)
            return;

        timeval timeout{ 1, 0 };
        ::setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    RouteSocket::~RouteSocket()
    {
        if (descriptor >= 0)
            ::close(descriptor);
    }

    bool RouteSocket::IsOpen() const
    {
        return descriptor >= 0;
    }

    std::optional<LinkState> RouteSocket::QueryLink(int interfaceIndex)
    {
        const auto answer = Exchange(EncodeGetLink(infra::MakeRange(request), interfaceIndex, ++sequence));
        return answer.has_value() ? DecodeLink(*answer) : std::nullopt;
    }

    int RouteSocket::SetLinkUp(int interfaceIndex, bool up)
    {
        return Acknowledge(EncodeSetLinkUp(infra::MakeRange(request), interfaceIndex, up, ++sequence));
    }

    int RouteSocket::SetBitrate(int interfaceIndex, uint32_t bitrate)
    {
        return Acknowledge(EncodeSetBitrate(infra::MakeRange(request), interfaceIndex, bitrate, ++sequence));
    }

    int RouteSocket::Acknowledge(std::size_t requestSize)
    {
        const auto answer = Exchange(requestSize);
        if (!answer.has_value())
            return errno != 0 ? errno : EIO;

        return DecodeAcknowledgement(*answer).value_or(EPROTO);
    }

    std::optional<infra::ConstByteRange> RouteSocket::Exchange(std::size_t requestSize)
    {
        if (descriptor < 0 || requestSize == 0)
            return std::nullopt;

        sockaddr_nl kernel{};
        kernel.nl_family = AF_NETLINK;
        if (::sendto(descriptor, request.data(), requestSize, 0, reinterpret_cast<const sockaddr*>(&kernel), sizeof(kernel)) != static_cast<ssize_t>(requestSize))
            return std::nullopt;

        const auto received = ::recv(descriptor, response.data(), response.size(), 0);
        if (received <= 0)
            return std::nullopt;

        return infra::ConstByteRange(response.data(), response.data() + received);
    }

    int ConfigureBitrate(int interfaceIndex, uint32_t bitrate)
    {
        RouteSocket route;
        if (!route.IsOpen())
            return errno != 0 ? errno : EIO;

        const auto link = route.QueryLink(interfaceIndex);
        if (link.has_value() && (!link->isCan || (link->up && link->bitrate == bitrate)))
            return 0;

        if (const auto error = route.SetLinkUp(interfaceIndex, false); error != 0)
            return error;

        if (const auto error = route.SetBitrate(interfaceIndex, bitrate); error != 0)
        {
            route.SetLinkUp(interfaceIndex, true);
            return error;
        }

        return route.SetLinkUp(interfaceIndex, true);
    }
}

#endif
