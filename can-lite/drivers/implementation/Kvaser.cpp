#ifdef _WIN32

#include "can-lite/drivers/implementation/Kvaser.hpp"
#include <algorithm>
#include <cstring>

namespace services
{
    KvaserAdapter::KvaserAdapter()
    {
        canInitializeLibrary();
        initialized = true;
    }

    KvaserAdapter::~KvaserAdapter()
    {
        Disconnect();
    }

    bool KvaserAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        int channel = 0;
        auto result = std::from_chars(interfaceName.data(), interfaceName.data() + interfaceName.size(), channel);
        if (result.ec != std::errc{})
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Invalid Kvaser channel number");
                });
            return false;
        }

        handle = canOpenChannel(channel, canOPEN_ACCEPT_VIRTUAL);
        if (handle < 0)
        {
            handle = canINVALID_HANDLE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to open Kvaser channel");
                });
            return false;
        }

        canBitrate_t canlibBitrate = BitrateToCanlib(bitrate);
        if (canSetBusParams(handle, canlibBitrate, 0, 0, 0, 0, 0) != canOK)
        {
            canClose(handle);
            handle = canINVALID_HANDLE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to set Kvaser bitrate");
                });
            return false;
        }

        if (canBusOn(handle) != canOK)
        {
            canClose(handle);
            handle = canINVALID_HANDLE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to go bus-on");
                });
            return false;
        }

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    void KvaserAdapter::Disconnect()
    {
        if (handle != canINVALID_HANDLE)
        {
            canBusOff(handle);
            canClose(handle);
            handle = canINVALID_HANDLE;

            NotifyObservers([](auto& observer)
                {
                    observer.OnConnectionChanged(false);
                });
        }
    }

    bool KvaserAdapter::IsConnected() const
    {
        return handle != canINVALID_HANDLE;
    }

    void KvaserAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected())
        {
            actionOnCompletion(false);
            return;
        }

        uint8_t buffer[8] = {};
        auto dlc = std::min(data.size(), static_cast<std::size_t>(8));
        std::memcpy(buffer, data.data(), dlc);

        canStatus status = canWrite(handle, static_cast<long>(id.Get29BitId()), buffer,
            static_cast<unsigned int>(dlc), canMSG_EXT);

        bool success = (status == canOK);

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
                    observer.OnError("Failed to send CAN frame via Kvaser");
                });
        }

        actionOnCompletion(success);
    }

    void KvaserAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    int KvaserAdapter::FileDescriptor() const
    {
        return static_cast<int>(handle);
    }

    void KvaserAdapter::ProcessReadEvent()
    {
        if (!IsConnected())
            return;

        long id = 0;
        uint8_t buffer[8] = {};
        unsigned int dlc = 0;
        unsigned int flags = 0;
        unsigned long timestamp = 0;

        canStatus status = canRead(handle, &id, buffer, &dlc, &flags, &timestamp);
        if (status != canOK)
            return;

        if (!(flags & canMSG_EXT))
            return;

        uint32_t rawId = static_cast<uint32_t>(id) & 0x1FFFFFFF;
        CanFrame data;
        for (unsigned int i = 0; i < dlc && i < 8; ++i)
            data.push_back(buffer[i]);

        NotifyObservers([rawId, &data](auto& observer)
            {
                observer.OnFrameLog(false, rawId, data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(rawId), data);
    }

    void KvaserAdapter::ValidateDriverAvailability() const
    {
        int channelCount = 0;
        if (canGetNumberOfChannels(&channelCount) != canOK || channelCount == 0)
            throw std::runtime_error(
                "Kvaser CANlib is not available or no channels found.\n\n"
                "Make sure the Kvaser drivers are installed:\n"
                "  https://www.kvaser.com/download/");
    }

    std::vector<std::string> KvaserAdapter::AvailableInterfaces() const
    {
        std::vector<std::string> result;
        int channelCount = 0;

        if (canGetNumberOfChannels(&channelCount) != canOK)
            return result;

        for (int i = 0; i < channelCount; ++i)
        {
            char name[256] = {};
            if (canGetChannelData(i, canCHANNELDATA_DEVDESCR_ASCII, name, sizeof(name)) == canOK)
                result.push_back(std::to_string(i) + ": " + name);
            else
                result.push_back(std::to_string(i));
        }

        return result;
    }

    canBitrate_t KvaserAdapter::BitrateToCanlib(uint32_t bitrate)
    {
        switch (bitrate)
        {
            case 1000000:
                return canBITRATE_1M;
            case 500000:
                return canBITRATE_500K;
            case 250000:
                return canBITRATE_250K;
            case 125000:
                return canBITRATE_125K;
            case 100000:
                return canBITRATE_100K;
            case 50000:
                return canBITRATE_50K;
            case 10000:
                return canBITRATE_10K;
            default:
                return canBITRATE_500K;
        }
    }
}

#endif
