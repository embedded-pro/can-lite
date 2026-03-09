#pragma once

#include "can-lite/drivers/interface/CanAdapter.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace services
{
    class CanableAdapter
        : public CanBusAdapter
    {
    public:
        CanableAdapter() = default;
        ~CanableAdapter() override;

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
        bool SendSlcanCommand(const char* command, std::size_t length);
        bool ParseSlcanFrame(const char* buffer, std::size_t length);
        static const char* BitrateToSlcanCode(uint32_t bitrate);

        HANDLE serialHandle = INVALID_HANDLE_VALUE;
        infra::Function<void(Id, const Message&)> receiveCallback;

        static constexpr std::size_t rxBufferSize = 64;
        char rxBuffer[rxBufferSize] = {};
        std::size_t rxPos = 0;
    };
}

#endif
