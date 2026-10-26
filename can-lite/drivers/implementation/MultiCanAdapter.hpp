#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/WithStorage.hpp"
#include <cstddef>
#include <optional>

namespace services
{
    class MultiCanAdapter
        : public CanBusAdapter
    {
    public:
        class Backend
            : public CanBusAdapterObserver
        {
        public:
            Backend(MultiCanAdapter& owner, CanBusAdapter& adapter, infra::BoundedConstString prefix, infra::BoundedConstString alias);

            void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
            void OnError(infra::BoundedConstString message) override;
            void OnConnectionChanged(bool connected) override;

            infra::BoundedConstString prefix;
            infra::BoundedConstString alias;

        private:
            bool IsActive() const;

            MultiCanAdapter& owner;
        };

        template<std::size_t Max>
        using WithMaxBackends = infra::WithStorage<MultiCanAdapter, infra::BoundedVector<Backend>::WithMaxSize<Max>>;

        static constexpr std::size_t maxInterfaceNameLength{ 64 };

        explicit MultiCanAdapter(infra::BoundedVector<Backend>& backends);
        ~MultiCanAdapter();

        void Add(infra::BoundedConstString prefix, CanBusAdapter& adapter, infra::BoundedConstString alias = {});

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
        struct Route
        {
            Backend& backend;
            infra::BoundedConstString channel;
        };

        std::optional<Route> Resolve(infra::BoundedConstString interfaceName) const;
        static bool IsUsable(const CanBusAdapter& adapter);
        void ReportUnknownInterface(infra::BoundedConstString interfaceName);
        void ScheduleCompletion(const infra::Function<void(bool)>& action, bool result);

        infra::BoundedVector<Backend>& backends;
        CanBusAdapter* active{};
        infra::Function<void(Id, const Message&)> receiveCallback;
        infra::Function<void(bool)> pendingCompletion;
        bool pendingSuccess{};
        infra::BoundedString::WithStorage<256> errorText;
    };
}
