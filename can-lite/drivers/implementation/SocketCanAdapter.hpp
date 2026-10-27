#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"

#ifdef __linux__

struct can_frame;

namespace services
{
    class SocketCanAdapter
        : public CanBusAdapter
    {
    public:
        SocketCanAdapter() = default;
        ~SocketCanAdapter();

        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        intptr_t FileDescriptor() const override;
        void ProcessReadEvent() override;

        void EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const override;
        void EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>& callback) const override;

    private:
        bool ReadFrame();
        static bool IsExtendedDataFrame(uint32_t canId);
        void Deliver(const struct can_frame& frame) const;
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        int socketDescriptor{ -1 };
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
    };
}

#endif
