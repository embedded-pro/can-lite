#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedDeque.hpp"
#include <cstddef>
#include <optional>

namespace integration
{
    class VirtualCan : public hal::Can
    {
    public:
        struct Frame
        {
            Id id;
            Message data;
        };

        void SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone) override;
        void ReceiveData(const infra::Function<void(Id, const Message&)>& callback) override;
        void Receive(Id id, const Message& data);
        void ConnectTo(VirtualCan& other);
        void InjectFrame(Id id, const Message& data);
        void Disconnect();
        void LoseFramesOn(Id id);
        void FailFramesOn(Id id);
        void DeliverEveryFrame();

        const Frame& LastSent() const;
        const infra::BoundedDeque<Frame>& SentFrames() const;

        int sendCount = 0;

    private:
        struct InFlight
        {
            Frame frame;
            infra::Function<void(bool)> onDone;
        };

        void DeliverOldest();

        static constexpr std::size_t recordedFrames = 64;
        static constexpr std::size_t framesInFlight = 4;

        VirtualCan* peer = nullptr;
        std::optional<Id> lostId;
        std::optional<Id> failedId;
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::BoundedDeque<Frame>::WithMaxSize<recordedFrames> sentFrames;
        infra::BoundedDeque<InFlight>::WithMaxSize<framesInFlight> inFlight;
    };
}
