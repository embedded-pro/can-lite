#ifdef _WIN32

#include "can-lite/drivers/implementation/PCan.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>

namespace services
{
    PCanAdapter::~PCanAdapter()
    {
        Disconnect();
    }

    bool PCanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        channel = ChannelFromName(interfaceName);
        if (channel == PCAN_NONEBUS)
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Invalid PCAN channel name");
                });
            return false;
        }

        TPCANBaudrate pcanBitrate = BitrateToPcan(bitrate);
        TPCANStatus status = CAN_Initialize(channel, pcanBitrate, 0, 0, 0);
        if (status != PCAN_ERROR_OK)
        {
            channel = PCAN_NONEBUS;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to initialize PCAN channel");
                });
            return false;
        }

        readEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (readEvent != INVALID_HANDLE_VALUE)
            CAN_SetValue(channel, PCAN_RECEIVE_EVENT, &readEvent, sizeof(readEvent));

        connected = true;
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    void PCanAdapter::Disconnect()
    {
        if (connected)
        {
            CAN_Uninitialize(channel);
            channel = PCAN_NONEBUS;

            if (readEvent != INVALID_HANDLE_VALUE)
            {
                CloseHandle(readEvent);
                readEvent = INVALID_HANDLE_VALUE;
            }

            connected = false;

            NotifyObservers([](auto& observer)
                {
                    observer.OnConnectionChanged(false);
                });
        }
    }

    bool PCanAdapter::IsConnected() const
    {
        return connected;
    }

    void PCanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected())
        {
            actionOnCompletion(false);
            return;
        }

        TPCANMsg msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.ID = id.Get29BitId();
        msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
        msg.LEN = static_cast<BYTE>(std::min(data.size(), static_cast<std::size_t>(8)));
        std::memcpy(msg.DATA, data.data(), msg.LEN);

        TPCANStatus status = CAN_Write(channel, &msg);
        bool success = (status == PCAN_ERROR_OK);

        if (success)
        {
            uint32_t rawId = id.Get29BitId();
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        }
        else
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to send CAN frame via PCAN");
                });
        }

        actionOnCompletion(success);
    }

    void PCanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    int PCanAdapter::FileDescriptor() const
    {
        return static_cast<int>(reinterpret_cast<intptr_t>(readEvent));
    }

    void PCanAdapter::ProcessReadEvent()
    {
        if (!IsConnected())
            return;

        TPCANMsg msg;
        TPCANTimestamp timestamp;

        TPCANStatus status = CAN_Read(channel, &msg, &timestamp);
        if (status != PCAN_ERROR_OK)
            return;

        if (!(msg.MSGTYPE & PCAN_MESSAGE_EXTENDED))
            return;

        uint32_t rawId = msg.ID & 0x1FFFFFFF;
        CanFrame data;
        for (BYTE i = 0; i < msg.LEN && i < 8; ++i)
            data.push_back(msg.DATA[i]);

        NotifyObservers([rawId, &data](auto& observer)
            {
                observer.OnFrameLog(false, rawId, data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(rawId), data);
    }

    void PCanAdapter::ValidateDriverAvailability() const
    {
        TPCANStatus status = CAN_Initialize(PCAN_USBBUS1, PCAN_BAUD_500K, 0, 0, 0);
        if (status == PCAN_ERROR_OK)
        {
            CAN_Uninitialize(PCAN_USBBUS1);
            return;
        }

        if (status == PCAN_ERROR_NODRIVER)
            throw std::runtime_error(
                "PCAN-Basic driver is not installed.\n\n"
                "Download from:\n"
                "  https://www.peak-system.com/PCAN-Basic.239.0.html");
    }

    std::vector<std::string> PCanAdapter::AvailableInterfaces() const
    {
        std::vector<std::string> result;

        static constexpr TPCANHandle usbChannels[] = {
            PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
            PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8
        };

        for (auto ch : usbChannels)
        {
            DWORD condition = 0;
            if (CAN_GetValue(ch, PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK)
            {
                if (condition & PCAN_CHANNEL_AVAILABLE)
                    result.push_back("USBBUS" + std::to_string(ch & 0xFF));
            }
        }

        return result;
    }

    TPCANBaudrate PCanAdapter::BitrateToPcan(uint32_t bitrate)
    {
        switch (bitrate)
        {
            case 1000000:
                return PCAN_BAUD_1M;
            case 800000:
                return PCAN_BAUD_800K;
            case 500000:
                return PCAN_BAUD_500K;
            case 250000:
                return PCAN_BAUD_250K;
            case 125000:
                return PCAN_BAUD_125K;
            case 100000:
                return PCAN_BAUD_100K;
            case 50000:
                return PCAN_BAUD_50K;
            case 20000:
                return PCAN_BAUD_20K;
            case 10000:
                return PCAN_BAUD_10K;
            default:
                return PCAN_BAUD_500K;
        }
    }

    TPCANHandle PCanAdapter::ChannelFromName(infra::BoundedConstString name)
    {
        if (name == "USBBUS1")
            return PCAN_USBBUS1;
        if (name == "USBBUS2")
            return PCAN_USBBUS2;
        if (name == "USBBUS3")
            return PCAN_USBBUS3;
        if (name == "USBBUS4")
            return PCAN_USBBUS4;
        if (name == "USBBUS5")
            return PCAN_USBBUS5;
        if (name == "USBBUS6")
            return PCAN_USBBUS6;
        if (name == "USBBUS7")
            return PCAN_USBBUS7;
        if (name == "USBBUS8")
            return PCAN_USBBUS8;
        return PCAN_NONEBUS;
    }
}

#endif
