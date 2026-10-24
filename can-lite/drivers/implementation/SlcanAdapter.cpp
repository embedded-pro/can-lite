#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <array>
#include <utility>

namespace services
{
    namespace
    {
        constexpr std::size_t readChunkSize{ 128 };
        constexpr std::size_t maxReadChunks{ 16 };

        infra::BoundedConstString OpenFailure(SerialPortOpenResult result)
        {
            switch (result)
            {
                case SerialPortOpenResult::notFound:
                    return "Serial port not found";
                case SerialPortOpenResult::accessDenied:
                    return "Access to the serial port was denied; on Linux, add the user to the dialout group";
                case SerialPortOpenResult::inUse:
                    return "The serial port is in use by another program";
                default:
                    return "The serial port could not be opened";
            }
        }
    }

    SlcanAdapter::SlcanAdapter(SerialPort& port)
        : port(port)
    {}

    SlcanAdapter::~SlcanAdapter()
    {
        Disconnect();
    }

    bool SlcanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        const auto code = SlcanBitrateCode(bitrate);
        if (!code)
        {
            ReportError("slcan supports 10, 20, 50, 100, 125, 250, 500 and 800 kbit/s and 1 Mbit/s");
            return false;
        }

        const auto result = port.Open(interfaceName);
        if (result != SerialPortOpenResult::opened)
        {
            ReportError(OpenFailure(result));
            return false;
        }

        if (!port.Write(SlcanOpenCommands(*code)))
        {
            port.Close();
            ReportError("Failed to configure the slcan adapter");
            return false;
        }

        decoder.Reset();
        awaitingCloseResponse = true;
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    void SlcanAdapter::Disconnect()
    {
        if (!IsConnected())
            return;

        port.Write(SlcanCloseCommand());
        port.Close();
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    bool SlcanAdapter::IsConnected() const
    {
        return port.IsOpen();
    }

    void SlcanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected() || !id.Is29BitId())
        {
            ScheduleCompletion(actionOnCompletion, false);
            return;
        }

        const auto rawId = id.Get29BitId();
        const bool success = port.Write(EncodeSlcanFrame(rawId, true, data));
        if (success)
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        else
            ReportError("Failed to send CAN frame via slcan");

        ScheduleCompletion(actionOnCompletion, success);
    }

    void SlcanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t SlcanAdapter::FileDescriptor() const
    {
        return port.Handle();
    }

    void SlcanAdapter::ProcessReadEvent()
    {
        for (std::size_t chunk = 0; chunk < maxReadChunks && IsConnected(); ++chunk)
            if (!ReadAvailable())
                return;
    }

    void SlcanAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        port.EnumeratePorts(callback);
    }

    void SlcanAdapter::EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>&) const
    {}

    bool SlcanAdapter::ReadAvailable()
    {
        std::array<uint8_t, readChunkSize> buffer{};
        const auto received = port.Read(infra::MakeRange(buffer));
        if (!received)
        {
            Lost();
            return false;
        }

        for (std::size_t index = 0; index < *received && IsConnected(); ++index)
            if (const auto item = decoder.Feed(static_cast<char>(buffer[index])))
                Dispatch(*item);

        return *received == buffer.size();
    }

    void SlcanAdapter::Dispatch(const SlcanDecoder::Item& item)
    {
        if (item.event != SlcanDecoder::Event::frame && std::exchange(awaitingCloseResponse, false))
            return;

        if (item.event == SlcanDecoder::Event::error)
            ReportError("The slcan adapter rejected a command");
        else if (item.event == SlcanDecoder::Event::frame && item.frame.extended && !item.frame.remote)
            Deliver(item.frame);
    }

    void SlcanAdapter::Deliver(const SlcanFrame& frame)
    {
        NotifyObservers([&frame](auto& observer)
            {
                observer.OnFrameLog(false, frame.id, frame.data);
            });

        if (receiveCallback)
            receiveCallback(hal::Can::Id::Create29BitId(frame.id), frame.data);
    }

    void SlcanAdapter::Lost()
    {
        port.Close();
        ReportError("The slcan adapter was disconnected");
        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    void SlcanAdapter::ReportError(infra::BoundedConstString message)
    {
        NotifyObservers([message](auto& observer)
            {
                observer.OnError(message);
            });
    }

    void SlcanAdapter::ScheduleCompletion(const infra::Function<void(bool)>& action, bool result)
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
