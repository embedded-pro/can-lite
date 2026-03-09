#pragma once

#include "hal/interfaces/Can.hpp"

namespace integration
{
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
