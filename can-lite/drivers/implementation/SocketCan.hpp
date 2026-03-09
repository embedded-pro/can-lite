#pragma once

#include "can-lite/drivers/interface/CanAdapter.hpp"

#ifdef __linux__

namespace services
{
    class SocketCanAdapter
        : public CanBusAdapter
    {
    public:
        SocketCanAdapter() = default;
        ~SocketCanAdapter() override;

        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        int FileDescriptor() const override;
        void ProcessReadEvent() override;

        std::vector<std::string> AvailableInterfaces() const override;
        void ValidateDriverAvailability() const override;

    private:
        int socketDescriptor = -1;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}

#endif
