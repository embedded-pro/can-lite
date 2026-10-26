#ifdef _WIN32

#include "can-lite/drivers/implementation/GsUsbAdapter.hpp"
#include "can-lite/drivers/codec/GsUsbProtocol.hpp"
#include "can-lite/drivers/implementation/ChannelNumber.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <setupapi.h>

namespace services
{
    namespace
    {
        constexpr GUID candleLightInterface{ 0xc15b4308, 0x04d3, 0x11e6, { 0xb3, 0xea, 0x60, 0x57, 0x18, 0x9e, 0x64, 0x43 } };
        constexpr UCHAR vendorToInterface{ 0x41 };
        constexpr UCHAR vendorFromInterface{ 0xc1 };
        constexpr USHORT channel{ 0 };
        constexpr ULONG writeTimeoutMs{ 100 };
        constexpr uint32_t echoSlots{ 32 };
        constexpr int maxTransfersPerPoll{ 64 };
        constexpr DWORD maxDevices{ 32 };

        template<class Visitor>
        void ForEachDevicePath(Visitor visit)
        {
            HDEVINFO devices{ SetupDiGetClassDevsW(&candleLightInterface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE) };
            if (devices == INVALID_HANDLE_VALUE)
                return;

            for (DWORD index = 0; index < maxDevices; ++index)
            {
                SP_DEVICE_INTERFACE_DATA interfaceData{};
                interfaceData.cbSize = sizeof(interfaceData);
                if (SetupDiEnumDeviceInterfaces(devices, nullptr, &candleLightInterface, index, &interfaceData) == FALSE)
                    break;

                alignas(SP_DEVICE_INTERFACE_DETAIL_DATA_W) std::array<uint8_t, 1024> storage{};
                auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
                if (SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, detail, static_cast<DWORD>(storage.size()), nullptr, nullptr) != FALSE && !visit(index, detail->DevicePath))
                    break;
            }

            SetupDiDestroyDeviceInfoList(devices);
        }
    }

    GsUsbAdapter::~GsUsbAdapter()
    {
        Disconnect();
    }

    bool GsUsbAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        const auto number = ParseChannelNumber(interfaceName);
        bool found{};
        if (number && *number >= 1)
            ForEachDevicePath([this, &found, number, bitrate](DWORD index, const wchar_t* path)
                {
                    if (index + 1 != *number)
                        return true;
                    found = true;
                    if (!Open(path, bitrate))
                        Close();
                    return false;
                });

        if (!found)
            ReportError("No such candleLight adapter is plugged in; candleLight adapters are numbered from 1 (for example candleLight:1)");
        if (!open)
            return false;

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    bool GsUsbAdapter::Open(const wchar_t* path, uint32_t bitrate)
    {
        device = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (device == INVALID_HANDLE_VALUE || WinUsb_Initialize(device, &usb) == FALSE)
        {
            ReportError("The candleLight adapter could not be opened; it may be in use by another program");
            return false;
        }

        if (!FindPipes())
        {
            ReportError("The candleLight adapter does not offer the expected endpoints");
            return false;
        }

        if (!Configure(bitrate))
            return false;

        ULONG timeout{ writeTimeoutMs };
        WinUsb_SetPipePolicy(usb, outPipe, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        readEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        writeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (readEvent == nullptr || writeEvent == nullptr || !StartRead())
        {
            ReportError("The candleLight adapter could not start receiving");
            return false;
        }

        open = true;
        return true;
    }

    bool GsUsbAdapter::Configure(uint32_t bitrate)
    {
        const auto hostFormat = gs_usb::EncodeHostFormat();
        std::array<uint8_t, gs_usb::bitTimingConstantsSize> constantsBytes{};
        if (!ControlOut(gs_usb::requestHostFormat, 1, hostFormat.data(), static_cast<uint16_t>(hostFormat.size())) || !ControlIn(gs_usb::requestBitTimingConstants, channel, constantsBytes.data(), static_cast<uint16_t>(constantsBytes.size())))
        {
            ReportError("The candleLight adapter did not answer the gs_usb setup requests");
            return false;
        }

        const auto constants = gs_usb::DecodeBitTimingConstants(constantsBytes.data(), constantsBytes.size());
        const auto timing = constants ? gs_usb::ComputeBitTiming(*constants, bitrate) : std::nullopt;
        if (!timing)
        {
            ReportError("The candleLight adapter cannot run at this bitrate");
            return false;
        }

        const auto timingBytes = gs_usb::EncodeBitTiming(*timing);
        const auto start = gs_usb::EncodeMode(gs_usb::modeStart);
        if (!ControlOut(gs_usb::requestBitTiming, channel, timingBytes.data(), static_cast<uint16_t>(timingBytes.size())) || !ControlOut(gs_usb::requestMode, channel, start.data(), static_cast<uint16_t>(start.size())))
        {
            ReportError("The candleLight adapter refused the bit timing or would not start");
            return false;
        }

        return true;
    }

    bool GsUsbAdapter::FindPipes()
    {
        USB_INTERFACE_DESCRIPTOR descriptor{};
        if (WinUsb_QueryInterfaceSettings(usb, 0, &descriptor) == FALSE)
            return false;

        for (UCHAR index = 0; index < descriptor.bNumEndpoints; ++index)
        {
            WINUSB_PIPE_INFORMATION pipe{};
            if (WinUsb_QueryPipe(usb, 0, index, &pipe) == FALSE || pipe.PipeType != UsbdPipeTypeBulk)
                continue;

            if ((pipe.PipeId & 0x80) != 0)
                inPipe = pipe.PipeId;
            else
                outPipe = pipe.PipeId;
        }

        return inPipe != 0 && outPipe != 0;
    }

    bool GsUsbAdapter::ControlOut(uint8_t request, uint16_t value, const uint8_t* data, uint16_t length)
    {
        WINUSB_SETUP_PACKET setup{ vendorToInterface, request, value, 0, length };
        ULONG transferred{};
        return WinUsb_ControlTransfer(usb, setup, const_cast<PUCHAR>(data), length, &transferred, nullptr) != FALSE;
    }

    bool GsUsbAdapter::ControlIn(uint8_t request, uint16_t value, uint8_t* data, uint16_t length)
    {
        WINUSB_SETUP_PACKET setup{ vendorFromInterface, request, value, 0, length };
        ULONG transferred{};
        return WinUsb_ControlTransfer(usb, setup, data, length, &transferred, nullptr) != FALSE && transferred == length;
    }

    bool GsUsbAdapter::StartRead()
    {
        ResetEvent(readEvent);
        readOverlapped = OVERLAPPED{};
        readOverlapped.hEvent = readEvent;
        if (WinUsb_ReadPipe(usb, inPipe, readBuffer.data(), static_cast<ULONG>(readBuffer.size()), nullptr, &readOverlapped) == FALSE && GetLastError() != ERROR_IO_PENDING)
            return false;

        readPending = true;
        return true;
    }

    void GsUsbAdapter::Disconnect()
    {
        if (!IsConnected())
            return;

        const auto reset = gs_usb::EncodeMode(gs_usb::modeReset);
        ControlOut(gs_usb::requestMode, channel, reset.data(), static_cast<uint16_t>(reset.size()));
        Close();
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    bool GsUsbAdapter::IsConnected() const
    {
        return open;
    }

    void GsUsbAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected() || !id.Is29BitId())
        {
            ScheduleCompletion(actionOnCompletion, false);
            return;
        }

        const auto rawId = id.Get29BitId();
        const bool success{ WriteFrame(rawId, data) };
        if (success)
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        else
            ReportError("Failed to send CAN frame via candleLight");

        ScheduleCompletion(actionOnCompletion, success);
    }

    bool GsUsbAdapter::WriteFrame(uint32_t rawId, const Message& data)
    {
        auto frame = gs_usb::EncodeFrame(nextEchoId, rawId, true, data);
        nextEchoId = (nextEchoId + 1) % echoSlots;

        ResetEvent(writeEvent);
        OVERLAPPED overlapped{};
        overlapped.hEvent = writeEvent;
        ULONG written{};
        const bool queued{ WinUsb_WritePipe(usb, outPipe, frame.data(), static_cast<ULONG>(frame.size()), nullptr, &overlapped) != FALSE || GetLastError() == ERROR_IO_PENDING };
        return queued && WinUsb_GetOverlappedResult(usb, &overlapped, &written, TRUE) != FALSE && written == frame.size();
    }

    void GsUsbAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t GsUsbAdapter::FileDescriptor() const
    {
        return readEvent != nullptr ? reinterpret_cast<intptr_t>(readEvent) : -1;
    }

    void GsUsbAdapter::ProcessReadEvent()
    {
        for (int count = 0; count < maxTransfersPerPoll && IsConnected(); ++count)
        {
            ULONG received{};
            if (WinUsb_GetOverlappedResult(usb, &readOverlapped, &received, FALSE) == FALSE)
            {
                if (GetLastError() != ERROR_IO_INCOMPLETE)
                    Lost("The candleLight adapter was disconnected");
                return;
            }

            readPending = false;
            Dispatch(readBuffer.data(), received);
            if (IsConnected() && !StartRead())
            {
                Lost("The candleLight adapter stopped responding");
                return;
            }
        }
    }

    void GsUsbAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        ForEachDevicePath([&callback](DWORD index, const wchar_t*)
            {
                ReportChannelNumber(index + 1, callback);
                return true;
            });
    }

    void GsUsbAdapter::EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>&) const
    {}

    void GsUsbAdapter::Dispatch(const uint8_t* bytes, std::size_t size)
    {
        for (std::size_t offset = 0; offset + gs_usb::frameSize <= size && IsConnected(); offset += gs_usb::frameSize)
        {
            const auto frame = gs_usb::DecodeFrame(bytes + offset, size - offset);
            if (!frame || frame->echoId != gs_usb::receivedEchoId || frame->remote)
                continue;

            if (frame->error)
            {
                ReportError("The candleLight adapter reported a CAN bus error");
                continue;
            }

            if (!frame->extended)
                continue;

            NotifyObservers([&frame](auto& observer)
                {
                    observer.OnFrameLog(false, frame->id, frame->data);
                });

            if (receiveCallback)
                receiveCallback(hal::Can::Id::Create29BitId(frame->id), frame->data);
        }
    }

    void GsUsbAdapter::Close()
    {
        if (usb != nullptr)
        {
            if (readPending)
            {
                WinUsb_AbortPipe(usb, inPipe);
                ULONG ignored{};
                WinUsb_GetOverlappedResult(usb, &readOverlapped, &ignored, TRUE);
            }
            WinUsb_Free(usb);
        }

        if (device != INVALID_HANDLE_VALUE)
            CloseHandle(device);
        for (auto* event : { readEvent, writeEvent })
            if (event != nullptr)
                CloseHandle(event);

        usb = nullptr;
        device = INVALID_HANDLE_VALUE;
        readEvent = nullptr;
        writeEvent = nullptr;
        readPending = false;
        open = false;
        inPipe = 0;
        outPipe = 0;
    }

    void GsUsbAdapter::Lost(infra::BoundedConstString message)
    {
        Close();
        ReportError(message);
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    void GsUsbAdapter::ReportError(infra::BoundedConstString message)
    {
        NotifyObservers([message](auto& observer)
            {
                observer.OnError(message);
            });
    }

    void GsUsbAdapter::ScheduleCompletion(const infra::Function<void(bool)>& action, bool result)
    {
        pendingCompletion = action;
        pendingSuccess = result;
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                if (pendingCompletion)
                    pendingCompletion(pendingSuccess);
            });
    }
}

#endif
