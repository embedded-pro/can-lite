#include "can-lite/testing/VirtualCan.hpp"

namespace services
{
    void VirtualCan::SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone)
    {
        lastSentId = id;
        lastSentData = data;
        sendCount++;

        if (peer)
            peer->Receive(id, data);

        onDone(true);
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
}
