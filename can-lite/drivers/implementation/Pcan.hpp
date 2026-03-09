#pragma once

#include "can-lite/drivers/interface/CanAdapter.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

        intptr_t FileDescriptor() const override;
        void ProcessReadEvent() override;

        void EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const override;
        bool IsDriverAvailable() const override;

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
