#pragma once

#include "can-lite/drivers/interface/CanAdapter.hpp"

#ifdef _WIN32

#include <PCANBasic.h>
#include <windows.h>

namespace services
{
    class PCanAdapter
        : public CanBusAdapter
    {
    public:
        PCanAdapter() = default;
        ~PCanAdapter() override;

        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        int FileDescriptor() const override;
        void ProcessReadEvent() override;

        std::vector<std::string> AvailableInterfaces() const override;
        void ValidateDriverAvailability() const override;

    private:
        static TPCANBaudrate BitrateToPcan(uint32_t bitrate);
        static TPCANHandle ChannelFromName(infra::BoundedConstString name);

        TPCANHandle channel = PCAN_NONEBUS;
        bool connected = false;
        HANDLE readEvent = INVALID_HANDLE_VALUE;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}

#endif
