#include "can-lite/drivers/implementation/SocketCanNetlink.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <linux/can/netlink.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <optional>

namespace
{
    using namespace services::socketcan;

    class LinkMessageBuilder
    {
    public:
        LinkMessageBuilder(uint16_t type, unsigned int linkFlags)
        {
            nlmsghdr header{};
            header.nlmsg_type = type;
            Put(header);

            ifinfomsg info{};
            info.ifi_flags = linkFlags;
            Put(info);
        }

        LinkMessageBuilder& LinkInfo(const char* kind, std::optional<uint32_t> bitrate)
        {
            const auto linkInfo = BeginNest(IFLA_LINKINFO);
            Attribute(IFLA_INFO_KIND, kind, std::strlen(kind) + 1);
            if (bitrate)
            {
                const auto data = BeginNest(IFLA_INFO_DATA);
                can_bittiming timing{};
                timing.bitrate = *bitrate;
                Attribute(IFLA_CAN_BITTIMING, &timing, sizeof(timing));
                EndNest(data);
            }
            EndNest(linkInfo);
            return *this;
        }

        infra::ConstByteRange Finish()
        {
            const auto length = static_cast<uint32_t>(size);
            std::memcpy(buffer.data(), &length, sizeof(length));
            return infra::ConstByteRange(buffer.data(), buffer.data() + size);
        }

    private:
        template<class T>
        void Put(const T& value)
        {
            PutBytes(&value, sizeof(value));
        }

        void PutBytes(const void* data, std::size_t length)
        {
            std::memcpy(buffer.data() + size, data, length);
            size += (length + 3u) & ~std::size_t{ 3u };
        }

        void Attribute(uint16_t type, const void* data, std::size_t length)
        {
            Put(rtattr{ static_cast<unsigned short>(sizeof(rtattr) + length), type });
            PutBytes(data, length);
        }

        std::size_t BeginNest(uint16_t type)
        {
            const auto start = size;
            Put(rtattr{ 0, type });
            return start;
        }

        void EndNest(std::size_t start)
        {
            const auto length = static_cast<unsigned short>(size - start);
            std::memcpy(buffer.data() + start, &length, sizeof(length));
        }

        std::array<uint8_t, 256> buffer{};
        std::size_t size{};
    };

    nlmsghdr HeaderOf(const std::array<uint8_t, 256>& buffer)
    {
        nlmsghdr header{};
        std::memcpy(&header, buffer.data(), sizeof(header));
        return header;
    }

    ifinfomsg InfoOf(const std::array<uint8_t, 256>& buffer)
    {
        ifinfomsg info{};
        std::memcpy(&info, buffer.data() + sizeof(nlmsghdr), sizeof(info));
        return info;
    }

    infra::ConstByteRange Acknowledgement(std::array<uint8_t, 64>& buffer, int error)
    {
        nlmsghdr header{};
        header.nlmsg_type = NLMSG_ERROR;
        header.nlmsg_len = sizeof(nlmsghdr) + sizeof(nlmsgerr);
        nlmsgerr body{};
        body.error = error;
        std::memcpy(buffer.data(), &header, sizeof(header));
        std::memcpy(buffer.data() + sizeof(header), &body, sizeof(body));
        return infra::ConstByteRange(buffer.data(), buffer.data() + header.nlmsg_len);
    }
}

TEST(TestSocketCanNetlink, asks_for_one_interface)
{
    std::array<uint8_t, 256> buffer{};
    const auto size = EncodeGetLink(infra::MakeRange(buffer), 7, 42);

    ASSERT_EQ(size, sizeof(nlmsghdr) + sizeof(ifinfomsg));
    EXPECT_EQ(HeaderOf(buffer).nlmsg_len, size);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_type, RTM_GETLINK);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_flags, NLM_F_REQUEST);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_seq, 42u);
    EXPECT_EQ(InfoOf(buffer).ifi_index, 7);
}

TEST(TestSocketCanNetlink, brings_a_link_up_or_down_and_asks_for_an_acknowledgement)
{
    std::array<uint8_t, 256> buffer{};

    ASSERT_NE(EncodeSetLinkUp(infra::MakeRange(buffer), 3, true, 1), 0u);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_type, RTM_NEWLINK);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_flags, NLM_F_REQUEST | NLM_F_ACK);
    EXPECT_EQ(InfoOf(buffer).ifi_flags, static_cast<unsigned int>(IFF_UP));
    EXPECT_EQ(InfoOf(buffer).ifi_change, static_cast<unsigned int>(IFF_UP));

    ASSERT_NE(EncodeSetLinkUp(infra::MakeRange(buffer), 3, false, 2), 0u);
    EXPECT_EQ(InfoOf(buffer).ifi_flags, 0u);
    EXPECT_EQ(InfoOf(buffer).ifi_change, static_cast<unsigned int>(IFF_UP));
}

TEST(TestSocketCanNetlink, sets_the_bitrate_through_the_can_link_data)
{
    std::array<uint8_t, 256> buffer{};
    const auto size = EncodeSetBitrate(infra::MakeRange(buffer), 3, 500000, 9);

    ASSERT_NE(size, 0u);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_len, size);
    EXPECT_EQ(HeaderOf(buffer).nlmsg_flags, NLM_F_REQUEST | NLM_F_ACK);
    EXPECT_EQ(InfoOf(buffer).ifi_index, 3);

    const auto decoded = DecodeLink(infra::ConstByteRange(buffer.data(), buffer.data() + size));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->isCan);
    EXPECT_EQ(decoded->bitrate, 500000u);
}

TEST(TestSocketCanNetlink, refuses_a_buffer_too_small_for_the_request)
{
    std::array<uint8_t, 40> buffer{};

    EXPECT_EQ(EncodeSetBitrate(infra::MakeRange(buffer), 3, 500000, 1), 0u);
}

TEST(TestSocketCanNetlink, reads_a_can_link_its_state_and_its_bitrate)
{
    LinkMessageBuilder message{ RTM_NEWLINK, IFF_UP };
    const auto decoded = DecodeLink(message.LinkInfo("can", 250000).Finish());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->isCan);
    EXPECT_TRUE(decoded->up);
    EXPECT_EQ(decoded->bitrate, 250000u);
}

TEST(TestSocketCanNetlink, a_can_link_without_bit_timing_has_no_bitrate)
{
    LinkMessageBuilder message{ RTM_NEWLINK, 0 };
    const auto decoded = DecodeLink(message.LinkInfo("can", std::nullopt).Finish());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->isCan);
    EXPECT_FALSE(decoded->up);
    EXPECT_FALSE(decoded->bitrate.has_value());
}

TEST(TestSocketCanNetlink, a_virtual_can_link_is_not_a_can_controller)
{
    LinkMessageBuilder message{ RTM_NEWLINK, IFF_UP };
    const auto decoded = DecodeLink(message.LinkInfo("vcan", std::nullopt).Finish());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_FALSE(decoded->isCan);
    EXPECT_FALSE(decoded->bitrate.has_value());
}

TEST(TestSocketCanNetlink, ignores_a_message_that_claims_more_bytes_than_it_has)
{
    LinkMessageBuilder message{ RTM_NEWLINK, IFF_UP };
    const auto whole = message.LinkInfo("can", 125000).Finish();

    EXPECT_FALSE(DecodeLink(infra::ConstByteRange(whole.begin(), whole.end() - 4)).has_value());
}

TEST(TestSocketCanNetlink, reads_the_kernels_acknowledgement)
{
    std::array<uint8_t, 64> buffer{};

    EXPECT_EQ(DecodeAcknowledgement(Acknowledgement(buffer, 0)), 0);
    EXPECT_EQ(DecodeAcknowledgement(Acknowledgement(buffer, -EPERM)), EPERM);

    LinkMessageBuilder notAnAcknowledgement{ RTM_NEWLINK, 0 };
    EXPECT_FALSE(DecodeAcknowledgement(notAnAcknowledgement.Finish()).has_value());
}

TEST(TestSocketCanNetlink, queries_the_loopback_link_from_the_kernel)
{
    RouteSocket route;
    ASSERT_TRUE(route.IsOpen());

    const auto loopback = route.QueryLink(static_cast<int>(if_nametoindex("lo")));

    ASSERT_TRUE(loopback.has_value());
    EXPECT_FALSE(loopback->isCan);
    EXPECT_TRUE(loopback->up);
}

TEST(TestSocketCanNetlink, leaves_a_link_without_bit_timing_alone)
{
    EXPECT_EQ(ConfigureBitrate(static_cast<int>(if_nametoindex("lo")), 500000), 0);
}
