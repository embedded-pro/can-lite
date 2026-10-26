#ifdef _WIN32

#include "can-lite/drivers/implementation/PcanAdapter.hpp"
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
        struct PcanMessage
        {
            DWORD id;
            BYTE type;
            BYTE length;
            BYTE data[8];
        };

        struct PcanTimestamp
        {
            DWORD millis;
            WORD millisOverflow;
            WORD micros;
        };

        constexpr const char* libraryName{ "PCANBasic.dll" };
        constexpr DWORD statusOk{ 0x00000 };
        constexpr BYTE parameterReceiveEvent{ 0x03 };
        constexpr BYTE parameterChannelCondition{ 0x0D };
        constexpr DWORD channelAvailable{ 0x01 };
        constexpr DWORD channelOccupied{ 0x02 };
        constexpr BYTE messageRemote{ 0x01 };
        constexpr BYTE messageExtended{ 0x02 };
        constexpr BYTE messageError{ 0x40 };
        constexpr BYTE messageStatus{ 0x80 };
        constexpr uint32_t maxChannels{ 16 };
        constexpr int maxFramesPerPoll{ 256 };

        constexpr std::array<std::pair<uint32_t, WORD>, 9> bitrateCodes{ {
            { 1000000, 0x0014 },
            { 800000, 0x0016 },
            { 500000, 0x001C },
            { 250000, 0x011C },
            { 125000, 0x031C },
            { 100000, 0x432F },
            { 50000, 0x472F },
            { 20000, 0x532F },
            { 10000, 0x672F },
        } };

        using Initialize = DWORD(WINAPI*)(WORD, WORD, BYTE, DWORD, WORD);
        using Uninitialize = DWORD(WINAPI*)(WORD);
        using Read = DWORD(WINAPI*)(WORD, PcanMessage*, PcanTimestamp*);
        using Write = DWORD(WINAPI*)(WORD, PcanMessage*);
        using GetValue = DWORD(WINAPI*)(WORD, BYTE, void*, DWORD);
        using SetValue = DWORD(WINAPI*)(WORD, BYTE, void*, DWORD);

        WORD UsbChannel(uint32_t oneBasedIndex)
        {
            return static_cast<WORD>(oneBasedIndex <= 8 ? 0x50 + oneBasedIndex : 0x500 + oneBasedIndex);
        }

        std::optional<WORD> BitrateCode(uint32_t bitrate)
        {
            for (const auto& [rate, code] : bitrateCodes)
                if (rate == bitrate)
                    return code;
            return std::nullopt;
        }
    }

    PcanAdapter::~PcanAdapter()
    {
        Disconnect();
    }

    bool PcanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        const auto index = ParseChannelNumber(interfaceName);
        const auto code = BitrateCode(bitrate);
        if (!index || *index < 1 || *index > maxChannels || !code)
        {
            ReportError("PCAN channels are numbered 1 to 16 (for example PCAN:1) and need a standard bitrate");
            return false;
        }

        if (!Open(UsbChannel(*index), *code))
            return false;

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    bool PcanAdapter::Open(WORD candidate, WORD bitrateCode)
    {
        library = LoadLibraryA(libraryName);
        const auto initialize = library != nullptr ? Load<Initialize>("CAN_Initialize") : nullptr;
        if (initialize == nullptr)
        {
            Release();
            ReportError("PCANBasic.dll was not found; install the PEAK-System PCAN driver package");
            return false;
        }

        if (initialize(candidate, bitrateCode, 0, 0, 0) != statusOk)
        {
            Release();
            ReportError("The PCAN channel could not be opened; check that the adapter is plugged in and not in use");
            return false;
        }

        channel = candidate;
        readEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        const auto setValue = Load<SetValue>("CAN_SetValue");
        if (readEvent != nullptr && (setValue == nullptr || setValue(channel, parameterReceiveEvent, &readEvent, sizeof(readEvent)) != statusOk))
        {
            CloseHandle(readEvent);
            readEvent = nullptr;
        }
        return true;
    }

    void PcanAdapter::Disconnect()
    {
        if (!IsConnected())
            return;

        Release();
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    bool PcanAdapter::IsConnected() const
    {
        return library != nullptr && channel != 0;
    }

    void PcanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        const auto write = IsConnected() && id.Is29BitId() ? Load<Write>("CAN_Write") : nullptr;
        if (write == nullptr)
        {
            ScheduleCompletion(actionOnCompletion, false);
            return;
        }

        PcanMessage message{};
        message.id = id.Get29BitId();
        message.type = messageExtended;
        message.length = static_cast<BYTE>(std::min<std::size_t>(data.size(), 8));
        std::memcpy(message.data, data.data(), message.length);

        const bool success{ write(channel, &message) == statusOk };
        if (success)
            NotifyObservers([rawId = message.id, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        else
            ReportError("Failed to send CAN frame via PCAN");

        ScheduleCompletion(actionOnCompletion, success);
    }

    void PcanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t PcanAdapter::FileDescriptor() const
    {
        return readEvent != nullptr ? reinterpret_cast<intptr_t>(readEvent) : -1;
    }

    void PcanAdapter::ProcessReadEvent()
    {
        const auto read = IsConnected() ? Load<Read>("CAN_Read") : nullptr;
        for (int count = 0; read != nullptr && count < maxFramesPerPoll && IsConnected(); ++count)
        {
            PcanMessage message{};
            PcanTimestamp timestamp{};
            if (read(channel, &message, &timestamp) != statusOk)
                return;

            if ((message.type & (messageStatus | messageError | messageRemote)) != 0 || (message.type & messageExtended) == 0)
                continue;

            CanFrame data;
            data.assign(message.data, message.data + std::min<BYTE>(message.length, 8));
            Deliver(message.id, data);
        }
    }

    void PcanAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        HMODULE probe{ LoadLibraryA(libraryName) };
        if (probe == nullptr)
            return;

        if (const auto getValue = reinterpret_cast<GetValue>(reinterpret_cast<void*>(GetProcAddress(probe, "CAN_GetValue"))))
            for (uint32_t index = 1; index <= maxChannels; ++index)
            {
                DWORD condition{};
                if (getValue(UsbChannel(index), parameterChannelCondition, &condition, sizeof(condition)) == statusOk && (condition == channelAvailable || condition == channelOccupied))
                    ReportChannelNumber(index, callback);
            }

        FreeLibrary(probe);
    }

    void PcanAdapter::EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>& callback) const
    {
        HMODULE probe{ LoadLibraryA(libraryName) };
        if (probe == nullptr)
            callback("PCAN: PCANBasic.dll was not found; install the PEAK-System PCAN driver package");
        else
            FreeLibrary(probe);
    }

    void PcanAdapter::Deliver(uint32_t id, const CanFrame& data)
    {
        NotifyObservers([id, &data](auto& observer)
            {
                observer.OnFrameLog(false, id, data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(id), data);
    }

    void PcanAdapter::Release()
    {
        if (library != nullptr && channel != 0)
            if (const auto uninitialize = Load<Uninitialize>("CAN_Uninitialize"))
                uninitialize(channel);

        if (readEvent != nullptr)
            CloseHandle(readEvent);
        if (library != nullptr)
            FreeLibrary(library);

        library = nullptr;
        channel = 0;
        readEvent = nullptr;
    }

    void PcanAdapter::ReportError(infra::BoundedConstString message)
    {
        NotifyObservers([message](auto& observer)
            {
                observer.OnError(message);
            });
    }

    void PcanAdapter::ScheduleCompletion(const infra::Function<void(bool)>& action, bool result)
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
