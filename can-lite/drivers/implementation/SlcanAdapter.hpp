#pragma once

#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "can-lite/drivers/interface/SerialPort.hpp"
#include "infra/timer/Timer.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    class SlcanAdapter
        : public CanBusAdapter
    {
    public:
        explicit SlcanAdapter(SerialPort& port);
        ~SlcanAdapter();

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
        enum class LinkState : uint8_t
        {
            closed,
            opening,
            open
        };

        bool ReadAvailable();
        void Dispatch(const SlcanDecoder::Item& item);
        void Handshake(const SlcanDecoder::Item& item);
        void CompleteHandshake();
        void FailHandshake(infra::BoundedConstString message);
        void Close(std::optional<infra::BoundedConstString> error);
        void Deliver(const SlcanFrame& frame) const;
        void Lost();
        void ReportError(infra::BoundedConstString message) const;
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        SerialPort& port;
        SlcanDecoder decoder;
        LinkState state{ LinkState::closed };
        uint8_t repliesReceived{};
        infra::TimerSingleShot handshakeTimer;
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
    };
}
