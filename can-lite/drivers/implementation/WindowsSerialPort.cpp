#ifdef _WIN32

#include "can-lite/drivers/implementation/WindowsSerialPort.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace services
{
    namespace
    {
        constexpr std::size_t maxPathLength{ 64 };
        constexpr DWORD writeTimeoutMs{ 50 };
        constexpr DWORD maxRegistryValues{ 256 };
        constexpr std::array<char, 4> devicePrefix{ '\\', '\\', '.', '\\' };

        SerialPortOpenResult ResultFor(DWORD error)
        {
            switch (error)
            {
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND:
                    return SerialPortOpenResult::notFound;
                case ERROR_ACCESS_DENIED:
                case ERROR_SHARING_VIOLATION:
                    return SerialPortOpenResult::inUse;
                default:
                    return SerialPortOpenResult::failed;
            }
        }

        void Configure(HANDLE handle)
        {
            DCB settings{};
            settings.DCBlength = sizeof(settings);
            GetCommState(handle, &settings);
            settings.BaudRate = CBR_115200;
            settings.ByteSize = 8;
            settings.Parity = NOPARITY;
            settings.StopBits = ONESTOPBIT;
            settings.fDtrControl = DTR_CONTROL_ENABLE;
            settings.fRtsControl = RTS_CONTROL_DISABLE;
            SetCommState(handle, &settings);

            COMMTIMEOUTS timeouts{};
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = writeTimeoutMs;
            SetCommTimeouts(handle, &timeouts);
            PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        }
    }

    WindowsSerialPort::~WindowsSerialPort()
    {
        Close();
    }

    SerialPortOpenResult WindowsSerialPort::Open(infra::BoundedConstString path)
    {
        Close();

        std::array<char, maxPathLength> devicePath{};
        if (path.empty() || devicePrefix.size() + path.size() >= devicePath.size())
            return SerialPortOpenResult::notFound;
        std::copy(path.begin(), path.end(), std::copy(devicePrefix.begin(), devicePrefix.end(), devicePath.begin()));

        handle = CreateFileA(devicePath.data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return ResultFor(GetLastError());

        Configure(handle);
        return SerialPortOpenResult::opened;
    }

    void WindowsSerialPort::Close()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }

    bool WindowsSerialPort::IsOpen() const
    {
        return handle != INVALID_HANDLE_VALUE;
    }

    bool WindowsSerialPort::Write(infra::BoundedConstString bytes)
    {
        DWORD written{};
        return IsOpen() && WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE && written == bytes.size();
    }

    std::optional<std::size_t> WindowsSerialPort::Read(infra::ByteRange buffer)
    {
        DWORD received{};
        if (!IsOpen() || ReadFile(handle, buffer.begin(), static_cast<DWORD>(buffer.size()), &received, nullptr) == FALSE)
            return std::nullopt;

        return static_cast<std::size_t>(received);
    }

    intptr_t WindowsSerialPort::Handle() const
    {
        return reinterpret_cast<intptr_t>(handle);
    }

    void WindowsSerialPort::EnumeratePorts(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        HKEY key{};
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS)
            return;

        for (DWORD index = 0; index < maxRegistryValues; ++index)
        {
            std::array<char, 256> name{};
            std::array<char, maxPathLength> port{};
            DWORD nameLength{ static_cast<DWORD>(name.size()) };
            DWORD portLength{ static_cast<DWORD>(port.size() - 1) };
            DWORD type{};
            if (RegEnumValueA(key, index, name.data(), &nameLength, nullptr, &type, reinterpret_cast<LPBYTE>(port.data()), &portLength) != ERROR_SUCCESS)
                break;

            if (type == REG_SZ)
                callback(infra::BoundedConstString(port.data(), std::strlen(port.data())));
        }

        RegCloseKey(key);
    }
}

#endif
