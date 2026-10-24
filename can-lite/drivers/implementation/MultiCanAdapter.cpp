#include "can-lite/drivers/implementation/MultiCanAdapter.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <algorithm>
#include <utility>

namespace services
{
    namespace
    {
        constexpr char separator{ ':' };
        constexpr std::size_t maxQuotedNameLength{ 48 };

        std::optional<infra::BoundedConstString> ChannelAfter(infra::BoundedConstString interfaceName, infra::BoundedConstString label)
        {
            if (label.empty() || interfaceName.size() <= label.size() || interfaceName[label.size()] != separator)
                return std::nullopt;

            if (interfaceName.substr(0, label.size()) != label)
                return std::nullopt;

            return interfaceName.substr(label.size() + 1);
        }
    }

    MultiCanAdapter::Backend::Backend(MultiCanAdapter& owner, CanBusAdapter& adapter, infra::BoundedConstString prefix, infra::BoundedConstString alias)
        : CanBusAdapterObserver(adapter)
        , prefix(prefix)
        , alias(alias)
        , owner(owner)
    {}

    void MultiCanAdapter::Backend::OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data)
    {
        if (IsActive())
            owner.NotifyObservers([transmitted, id, &data](auto& observer)
                {
                    observer.OnFrameLog(transmitted, id, data);
                });
    }

    void MultiCanAdapter::Backend::OnError(infra::BoundedConstString message)
    {
        if (IsActive())
            owner.NotifyObservers([message](auto& observer)
                {
                    observer.OnError(message);
                });
    }

    void MultiCanAdapter::Backend::OnConnectionChanged(bool connected)
    {
        if (!IsActive())
            return;

        if (!connected)
            owner.active = nullptr;

        owner.NotifyObservers([connected](auto& observer)
            {
                observer.OnConnectionChanged(connected);
            });
    }

    bool MultiCanAdapter::Backend::IsActive() const
    {
        return owner.active == &Subject();
    }

    MultiCanAdapter::MultiCanAdapter(infra::BoundedVector<Backend>& backends)
        : backends(backends)
    {}

    MultiCanAdapter::~MultiCanAdapter()
    {
        for (auto& backend : backends)
            backend.Subject().ReceiveData(nullptr);
        backends.clear();
    }

    void MultiCanAdapter::Add(infra::BoundedConstString prefix, CanBusAdapter& adapter, infra::BoundedConstString alias)
    {
        adapter.ReceiveData([this, &adapter](Id id, const Message& data)
            {
                if (active == &adapter && receiveCallback)
                    receiveCallback(id, data);
            });

        backends.emplace_back(*this, adapter, prefix, alias);
    }

    bool MultiCanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
    {
        if (IsConnected())
            Disconnect();

        const auto route = Resolve(interfaceName);
        if (!route)
        {
            ReportUnknownInterface(interfaceName);
            return false;
        }

        active = &route->backend.Subject();
        if (!active->Connect(route->channel, bitrate))
        {
            active = nullptr;
            return false;
        }

        return true;
    }

    void MultiCanAdapter::Disconnect()
    {
        if (active != nullptr)
            active->Disconnect();
        active = nullptr;
    }

    bool MultiCanAdapter::IsConnected() const
    {
        return active != nullptr && active->IsConnected();
    }

    void MultiCanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (active == nullptr)
            ScheduleCompletion(actionOnCompletion, false);
        else
            active->SendData(id, data, actionOnCompletion);
    }

    void MultiCanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    intptr_t MultiCanAdapter::FileDescriptor() const
    {
        return active != nullptr ? active->FileDescriptor() : -1;
    }

    void MultiCanAdapter::ProcessReadEvent()
    {
        if (active != nullptr)
            active->ProcessReadEvent();
    }

    void MultiCanAdapter::EnumerateInterfaces(const infra::Function<void(infra::BoundedConstString)>& callback) const
    {
        for (const auto& backend : backends)
            backend.Subject().EnumerateInterfaces([&backend, &callback](infra::BoundedConstString channel)
                {
                    if (backend.prefix.empty())
                    {
                        callback(channel);
                        return;
                    }

                    infra::BoundedString::WithStorage<maxInterfaceNameLength> name{ backend.prefix };
                    name.push_back(separator);
                    name.append(channel.substr(0, name.max_size() - name.size()));
                    callback(name);
                });
    }

    void MultiCanAdapter::EnumerateMissingDrivers(const infra::Function<void(infra::BoundedConstString reason)>& callback) const
    {
        if (std::any_of(backends.begin(), backends.end(), [](const Backend& backend)
                {
                    return IsUsable(backend.Subject());
                }))
            return;

        for (const auto& backend : backends)
            backend.Subject().EnumerateMissingDrivers(callback);
    }

    std::optional<MultiCanAdapter::Route> MultiCanAdapter::Resolve(infra::BoundedConstString interfaceName) const
    {
        Backend* fallback{};
        for (auto& backend : backends)
        {
            for (const auto& label : { backend.prefix, backend.alias })
                if (const auto channel = ChannelAfter(interfaceName, label))
                    return Route{ backend, *channel };

            if (backend.prefix.empty())
                fallback = &backend;
        }

        if (fallback != nullptr && interfaceName.find(separator) == infra::BoundedConstString::npos)
            return Route{ *fallback, interfaceName };

        return std::nullopt;
    }

    bool MultiCanAdapter::IsUsable(const CanBusAdapter& adapter)
    {
        bool missingDriver{};
        bool hasInterface{};
        adapter.EnumerateMissingDrivers([&missingDriver](infra::BoundedConstString)
            {
                missingDriver = true;
            });
        adapter.EnumerateInterfaces([&hasInterface](infra::BoundedConstString)
            {
                hasInterface = true;
            });
        return !missingDriver && hasInterface;
    }

    void MultiCanAdapter::ReportUnknownInterface(infra::BoundedConstString interfaceName)
    {
        errorText = "Unknown interface \"";
        errorText.append(interfaceName.substr(0, maxQuotedNameLength));
        errorText.append("\"; use one of: ");
        bool first{ true };
        for (const auto& backend : backends)
        {
            if (!std::exchange(first, false))
                errorText.append(", ");
            if (backend.prefix.empty())
                errorText.append("<interface>");
            else
                errorText.append(backend.prefix.substr(0, maxQuotedNameLength)).append(":<channel>");
        }

        NotifyObservers([this](auto& observer)
            {
                observer.OnError(errorText);
            });
    }

    void MultiCanAdapter::ScheduleCompletion(const infra::Function<void(bool)>& action, bool result)
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
