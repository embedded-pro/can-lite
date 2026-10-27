#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"

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
    class KvaserAdapter
        : public CanBusAdapter
    {
    public:
        KvaserAdapter() = default;
        ~KvaserAdapter();

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
        template<class Function>
        Function Load(const char* name) const
        {
            return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(library, name)));
        }

        static HMODULE LoadCanlib();
        static void UnloadCanlib(HMODULE canlib);
        bool Open(int channelNumber, long bitrateCode);
        void Deliver(uint32_t id, const CanFrame& data);
        void Release();
        void ReportError(infra::BoundedConstString message);
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        HMODULE library{};
        int handle{ -1 };
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
    };
}

#endif
