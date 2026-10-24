#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <array>
#include <cstdint>
#include <windows.h>
#include <winusb.h>

namespace services
{
    class GsUsbAdapter
        : public CanBusAdapter
    {
    public:
        GsUsbAdapter() = default;
        ~GsUsbAdapter() override;

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
        bool Open(const wchar_t* path, uint32_t bitrate);
        bool Configure(uint32_t bitrate);
        bool FindPipes();
        bool ControlOut(uint8_t request, uint16_t value, const uint8_t* data, uint16_t length);
        bool ControlIn(uint8_t request, uint16_t value, uint8_t* data, uint16_t length);
        bool StartRead();
        bool WriteFrame(uint32_t rawId, const Message& data);
        void Dispatch(const uint8_t* bytes, std::size_t size);
        void Close();
        void Lost(infra::BoundedConstString message);
        void ReportError(infra::BoundedConstString message);
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        HANDLE device{ INVALID_HANDLE_VALUE };
        WINUSB_INTERFACE_HANDLE usb{};
        UCHAR inPipe{};
        UCHAR outPipe{};
        HANDLE readEvent{};
        HANDLE writeEvent{};
        OVERLAPPED readOverlapped{};
        std::array<uint8_t, 64> readBuffer{};
        bool readPending{};
        bool open{};
        uint32_t nextEchoId{};
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
    };
}

#endif
