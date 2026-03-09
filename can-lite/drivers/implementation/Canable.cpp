#ifdef _WIN32

#include "can-lite/drivers/implementation/Canable.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace services
{
    namespace
    {
        uint8_t HexCharToNibble(char c)
        {
            if (c >= '0' && c <= '9')
                return static_cast<uint8_t>(c - '0');
            if (c >= 'A' && c <= 'F')
                return static_cast<uint8_t>(c - 'A' + 10);
            if (c >= 'a' && c <= 'f')
                return static_cast<uint8_t>(c - 'a' + 10);
            return 0;
        }

        char NibbleToHex(uint8_t nibble)
        {
            static constexpr char hexChars[] = "0123456789ABCDEF";
            return hexChars[nibble & 0x0F];
        }
    }

    CanableAdapter::~CanableAdapter()
    {
        Disconnect();
    }

    bool CanableAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        char portPath[16] = {};
        std::snprintf(portPath, sizeof(portPath), "\\\\.\\%.*s",
            static_cast<int>(interfaceName.size()), interfaceName.data());

        serialHandle = CreateFileA(portPath, GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (serialHandle == INVALID_HANDLE_VALUE)
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to open CANable serial port");
                });
            return false;
        }

        DCB dcb;
        std::memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(serialHandle, &dcb))
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to get serial port state");
                });
            return false;
        }

        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;

        if (!SetCommState(serialHandle, &dcb))
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to configure serial port");
                });
            return false;
        }

        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = 1;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 1;
        SetCommTimeouts(serialHandle, &timeouts);

        SendSlcanCommand("C\r", 2);

        const char* bitrateCode = BitrateToSlcanCode(bitrate);
        if (bitrateCode == nullptr)
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Unsupported bitrate for SLCAN");
                });
            return false;
        }

        char bitrateCmd[4] = { 'S', bitrateCode[0], '\r', '\0' };
        if (!SendSlcanCommand(bitrateCmd, 3))
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to set SLCAN bitrate");
                });
            return false;
        }

        if (!SendSlcanCommand("O\r", 2))
        {
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to open SLCAN channel");
                });
            return false;
        }

        rxPos = 0;

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    void CanableAdapter::Disconnect()
    {
        if (serialHandle != INVALID_HANDLE_VALUE)
        {
            SendSlcanCommand("C\r", 2);
            CloseHandle(serialHandle);
            serialHandle = INVALID_HANDLE_VALUE;

            NotifyObservers([](auto& observer)
                {
                    observer.OnConnectionChanged(false);
                });
        }
    }

    bool CanableAdapter::IsConnected() const
    {
        return serialHandle != INVALID_HANDLE_VALUE;
    }

    void CanableAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected())
        {
            actionOnCompletion(false);
            return;
        }

        // SLCAN extended frame: T<8-hex-id><dlc><hex-data>\r
        char cmd[32] = {};
        std::size_t pos = 0;

        cmd[pos++] = 'T';

        uint32_t rawId = id.Get29BitId();
        for (int i = 7; i >= 0; --i)
            cmd[pos++] = NibbleToHex(static_cast<uint8_t>((rawId >> (i * 4)) & 0x0F));

        auto dlc = std::min(data.size(), static_cast<std::size_t>(8));
        cmd[pos++] = static_cast<char>('0' + dlc);

        for (std::size_t i = 0; i < dlc; ++i)
        {
            cmd[pos++] = NibbleToHex(data[i] >> 4);
            cmd[pos++] = NibbleToHex(data[i] & 0x0F);
        }

        cmd[pos++] = '\r';

        bool success = SendSlcanCommand(cmd, pos);

        if (success)
        {
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        }
        else
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to send CAN frame via CANable");
                });
        }

        actionOnCompletion(success);
    }

    void CanableAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t CanableAdapter::FileDescriptor() const
    {
        return reinterpret_cast<intptr_t>(serialHandle);
    }

    void CanableAdapter::ProcessReadEvent()
    {
        if (!IsConnected())
            return;

        uint8_t byte = 0;
        DWORD bytesRead = 0;

        while (ReadFile(serialHandle, &byte, 1, &bytesRead, nullptr) && bytesRead == 1)
        {
            if (byte == '\r')
            {
                if (rxPos > 0)
                {
                    ParseSlcanFrame(rxBuffer, rxPos);
                    rxPos = 0;
                }
            }
            else
            {
                if (rxPos < rxBufferSize - 1)
                    rxBuffer[rxPos++] = static_cast<char>(byte);
                else
                    rxPos = 0;
            }
        }
    }

    bool CanableAdapter::SendSlcanCommand(const char* command, std::size_t length)
    {
        DWORD bytesWritten = 0;
        return WriteFile(serialHandle, command, static_cast<DWORD>(length), &bytesWritten, nullptr) && bytesWritten == static_cast<DWORD>(length);
    }

    bool CanableAdapter::ParseSlcanFrame(const char* buffer, std::size_t length)
    {
        // Extended frame: T<8-hex-id><dlc><hex-data>
        if (length < 10 || buffer[0] != 'T')
            return false;

        uint32_t rawId = 0;
        for (std::size_t i = 1; i <= 8; ++i)
            rawId = (rawId << 4) | HexCharToNibble(buffer[i]);

        rawId &= 0x1FFFFFFF;

        uint8_t dlc = static_cast<uint8_t>(buffer[9] - '0');
        if (dlc > 8)
            return false;

        if (length < 10 + static_cast<std::size_t>(dlc) * 2)
            return false;

        CanFrame data;
        for (uint8_t i = 0; i < dlc; ++i)
        {
            uint8_t hi = HexCharToNibble(buffer[10 + i * 2]);
            uint8_t lo = HexCharToNibble(buffer[11 + i * 2]);
            data.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }

        NotifyObservers([rawId, &data](auto& observer)
            {
                observer.OnFrameLog(false, rawId, data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(rawId), data);

        return true;
    }

    bool CanableAdapter::IsDriverAvailable() const
    {
        for (int i = 1; i <= 256; ++i)
        {
            char portName[16];
            std::snprintf(portName, sizeof(portName), "\\\\.\\COM%d", i);
            HANDLE h = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                CloseHandle(h);
                return true;
            }
        }

        return false;
    }

    void CanableAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        for (int i = 1; i <= 256; ++i)
        {
            char portName[16];
            std::snprintf(portName, sizeof(portName), "\\\\.\\COM%d", i);
            HANDLE h = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                CloseHandle(h);
                char displayName[8];
                std::snprintf(displayName, sizeof(displayName), "COM%d", i);
                callback(displayName);
            }
        }
    }

    const char* CanableAdapter::BitrateToSlcanCode(uint32_t bitrate)
    {
        switch (bitrate)
        {
            case 10000:
                return "0";
            case 20000:
                return "1";
            case 50000:
                return "2";
            case 100000:
                return "3";
            case 125000:
                return "4";
            case 250000:
                return "5";
            case 500000:
                return "6";
            case 800000:
                return "7";
            case 1000000:
                return "8";
            default:
                return nullptr;
        }
    }
}

#endif
