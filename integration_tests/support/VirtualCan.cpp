#include "support/VirtualCan.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace integration
{
    void VirtualCan::SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone)
    {
        really_assert(!inFlight.full());

        if (sentFrames.full())
            sentFrames.pop_front();
        sentFrames.push_back(Frame{ id, data });
        sendCount++;

        inFlight.push_back(InFlight{ Frame{ id, data }, onDone });
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                DeliverOldest();
            });
    }

    void VirtualCan::ReceiveData(const infra::Function<void(Id, const Message&)>& callback)
    {
        receiveCallback = callback;
    }

    void VirtualCan::Receive(Id id, const Message& data)
    {
        if (receiveCallback)
            receiveCallback(id, data);
    }

    void VirtualCan::ConnectTo(VirtualCan& other)
    {
        peer = &other;
        other.peer = this;
    }

    void VirtualCan::InjectFrame(Id id, const Message& data)
    {
        if (receiveCallback)
            receiveCallback(id, data);
    }

    void VirtualCan::Disconnect()
    {
        if (peer)
            peer->peer = nullptr;

        peer = nullptr;
    }

    const VirtualCan::Frame& VirtualCan::LastSent() const
    {
        really_assert(!sentFrames.empty());
        return sentFrames.back();
    }

    const infra::BoundedDeque<VirtualCan::Frame>& VirtualCan::SentFrames() const
    {
        return sentFrames;
    }

    void VirtualCan::DeliverOldest()
    {
        auto delivery = inFlight.front();
        inFlight.pop_front();

        if (peer)
            peer->Receive(delivery.frame.id, delivery.frame.data);

        delivery.onDone(true);
    }
}
