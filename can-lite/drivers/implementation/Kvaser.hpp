#pragma once

#include "can-lite/drivers/interface/CanAdapter.hpp"

#ifdef _WIN32

#include <canlib.h>

namespace services
{
    class KvaserAdapter
        : public CanBusAdapter
    {
    public:
        KvaserAdapter();
        ~KvaserAdapter() override;

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
        static canBitrate_t BitrateToCanlib(uint32_t bitrate);

        canHandle handle = canINVALID_HANDLE;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}

#endif
