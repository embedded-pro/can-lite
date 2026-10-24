#pragma once

#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "can-lite/drivers/interface/SerialPort.hpp"

namespace services
{
    class SlcanAdapter
        : public CanBusAdapter
    {
    public:
        explicit SlcanAdapter(SerialPort& port);
        ~SlcanAdapter() override;

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
        bool ReadAvailable();
        void Dispatch(const SlcanDecoder::Item& item);
        void Deliver(const SlcanFrame& frame);
        void Lost();
        void ReportError(infra::BoundedConstString message);
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        SerialPort& port;
        SlcanDecoder decoder;
        bool awaitingCloseResponse{};
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
    };
}
