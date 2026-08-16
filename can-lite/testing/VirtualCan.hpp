#pragma once

#include "hal/interfaces/Can.hpp"

namespace services
{
    // A two-node bus in memory: whatever one side sends, the other receives.
    // Published alongside the echo category so a consumer can compose the same
    // host-side test rig for its own categories.
    class VirtualCan : public hal::Can
    {
    public:
        void SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone) override;
        void ReceiveData(const infra::Function<void(Id, const Message&)>& callback) override;
        void Receive(Id id, const Message& data);
        void ConnectTo(VirtualCan& other);
        void InjectFrame(Id id, const Message& data);

        Id lastSentId = Id::Create29BitId(0);
        Message lastSentData;
        int sendCount = 0;

    private:
        VirtualCan* peer = nullptr;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}
