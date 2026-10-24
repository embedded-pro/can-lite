#ifdef _WIN32

#include "can-lite/drivers/implementation/KvaserAdapter.hpp"
#include "can-lite/drivers/implementation/ChannelNumber.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace services
{
    namespace
    {
        constexpr int statusOk{ 0 };
        constexpr int openExclusive{ 0x0008 };
        constexpr unsigned int messageRemote{ 0x0001 };
        constexpr unsigned int messageExtended{ 0x0004 };
        constexpr unsigned int messageErrorFrame{ 0x0020 };
        constexpr int maxFramesPerPoll{ 256 };
        constexpr int maxChannels{ 64 };

        constexpr std::array<std::pair<uint32_t, long>, 7> bitrateCodes{ {
            { 1000000, -1 },
            { 500000, -2 },
            { 250000, -3 },
            { 125000, -4 },
            { 100000, -5 },
            { 50000, -7 },
            { 10000, -9 },
        } };

        using InitializeLibrary = void(WINAPI*)();
        using GetNumberOfChannels = int(WINAPI*)(int*);
        using OpenChannel = int(WINAPI*)(int, int);
        using SetBusParams = int(WINAPI*)(int, long, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
        using BusOn = int(WINAPI*)(int);
        using BusOff = int(WINAPI*)(int);
        using Close = int(WINAPI*)(int);
        using Write = int(WINAPI*)(int, long, void*, unsigned int, unsigned int);
        using Read = int(WINAPI*)(int, long*, void*, unsigned int*, unsigned int*, unsigned long*);

        template<class Function>
        Function LoadFrom(HMODULE module, const char* name)
        {
            return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(module, name)));
        }

        std::optional<long> BitrateCode(uint32_t bitrate)
        {
            for (const auto& [rate, code] : bitrateCodes)
                if (rate == bitrate)
                    return code;
            return std::nullopt;
        }
    }

    KvaserAdapter::~KvaserAdapter()
    {
        Disconnect();
    }

    HMODULE KvaserAdapter::LoadCanlib()
    {
        HMODULE canlib = LoadLibraryA("canlib32.dll");
        if (canlib == nullptr)
            return nullptr;

        if (const auto initialize = LoadFrom<InitializeLibrary>(canlib, "canInitializeLibrary"))
            initialize();
        return canlib;
    }

    bool KvaserAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        const auto number = ParseChannelNumber(interfaceName);
        const auto code = BitrateCode(bitrate);
        if (!number || *number >= static_cast<uint32_t>(maxChannels) || !code)
        {
            ReportError("Kvaser channels are numbered from 0 (for example Kvaser:0) and need a standard bitrate");
            return false;
        }

        if (!Open(static_cast<int>(*number), *code))
            return false;

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    bool KvaserAdapter::Open(int channelNumber, long bitrateCode)
    {
        library = LoadCanlib();
        const auto open = library != nullptr ? Load<OpenChannel>("canOpenChannel") : nullptr;
        const auto setBus = library != nullptr ? Load<SetBusParams>("canSetBusParams") : nullptr;
        const auto busOn = library != nullptr ? Load<BusOn>("canBusOn") : nullptr;
        if (open == nullptr || setBus == nullptr || busOn == nullptr)
        {
            Release();
            ReportError("canlib32.dll was not found; install the Kvaser CANlib driver");
            return false;
        }

        handle = open(channelNumber, openExclusive);
        if (handle < 0 || setBus(handle, bitrateCode, 0, 0, 0, 0, 0) != statusOk || busOn(handle) != statusOk)
        {
            Release();
            ReportError("The Kvaser channel could not be opened; check that the adapter is plugged in and not in use");
            return false;
        }

        return true;
    }

    void KvaserAdapter::Disconnect()
    {
        if (!IsConnected())
            return;

        Release();
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    bool KvaserAdapter::IsConnected() const
    {
        return library != nullptr && handle >= 0;
    }

    void KvaserAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        const auto write = IsConnected() && id.Is29BitId() ? Load<Write>("canWrite") : nullptr;
        if (write == nullptr)
        {
            ScheduleCompletion(actionOnCompletion, false);
            return;
        }

        const auto rawId = id.Get29BitId();
        std::array<BYTE, 8> buffer{};
        const auto length = static_cast<unsigned int>(std::min<std::size_t>(data.size(), buffer.size()));
        std::memcpy(buffer.data(), data.data(), length);

        const bool success = write(handle, static_cast<long>(rawId), buffer.data(), length, messageExtended) == statusOk;
        if (success)
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        else
            ReportError("Failed to send CAN frame via Kvaser");

        ScheduleCompletion(actionOnCompletion, success);
    }

    void KvaserAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t KvaserAdapter::FileDescriptor() const
    {
        return -1;
    }

    void KvaserAdapter::ProcessReadEvent()
    {
        const auto read = IsConnected() ? Load<Read>("canRead") : nullptr;
        for (int count = 0; read != nullptr && count < maxFramesPerPoll && IsConnected(); ++count)
        {
            long rawId{};
            std::array<BYTE, 8> buffer{};
            unsigned int length{};
            unsigned int flags{};
            unsigned long time{};
            if (read(handle, &rawId, buffer.data(), &length, &flags, &time) != statusOk)
                return;

            if ((flags & (messageRemote | messageErrorFrame)) != 0 || (flags & messageExtended) == 0)
                continue;

            CanFrame data;
            data.assign(buffer.begin(), buffer.begin() + std::min<unsigned int>(length, 8));
            Deliver(static_cast<uint32_t>(rawId), data);
        }
    }

    void KvaserAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        HMODULE probe = LoadCanlib();
        if (probe == nullptr)
            return;

        int count{};
        if (const auto getCount = LoadFrom<GetNumberOfChannels>(probe, "canGetNumberOfChannels"); getCount != nullptr && getCount(&count) == statusOk)
            for (int index = 0; index < std::min(count, maxChannels); ++index)
                ReportChannelNumber(static_cast<uint32_t>(index), callback);

        FreeLibrary(probe);
    }

    void KvaserAdapter::EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>& callback) const
    {
        HMODULE probe = LoadLibraryA("canlib32.dll");
        if (probe == nullptr)
            callback("Kvaser: canlib32.dll was not found; install the Kvaser CANlib driver");
        else
            FreeLibrary(probe);
    }

    void KvaserAdapter::Deliver(uint32_t id, const CanFrame& data)
    {
        NotifyObservers([id, &data](auto& observer)
            {
                observer.OnFrameLog(false, id, data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(id), data);
    }

    void KvaserAdapter::Release()
    {
        if (library != nullptr && handle >= 0)
        {
            if (const auto busOff = Load<BusOff>("canBusOff"))
                busOff(handle);
            if (const auto close = Load<Close>("canClose"))
                close(handle);
        }

        if (library != nullptr)
            FreeLibrary(library);
        library = nullptr;
        handle = -1;
    }

    void KvaserAdapter::ReportError(infra::BoundedConstString message)
    {
        NotifyObservers([message](auto& observer)
            {
                observer.OnError(message);
            });
    }

    void KvaserAdapter::ScheduleCompletion(const infra::Function<void(bool)>& action, bool result)
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
