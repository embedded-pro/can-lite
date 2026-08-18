#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace integration
{
    static constexpr uint8_t demoCategoryId = 0x03;

    static constexpr uint8_t demoPingId = 0x00;
    static constexpr uint8_t demoSetParametersId = 0x01;
    static constexpr uint8_t demoQueryValueId = 0x02;
    static constexpr uint8_t demoFailId = 0x03;

    static constexpr uint8_t demoValueResponseId = 0x82;

    enum class DemoError : uint8_t
    {
        notSupported = 0,
        busy = 1
    };

    struct DemoParameters
    {
        int16_t first;
        int16_t second;
        int16_t third;
    };

    class DemoCategoryServer;

    class DemoCategoryServerObserver
        : public infra::SingleObserver<DemoCategoryServerObserver, DemoCategoryServer>
    {
    public:
        using infra::SingleObserver<DemoCategoryServerObserver, DemoCategoryServer>::SingleObserver;

        virtual void OnPing(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetParameters(const DemoParameters& parameters, const infra::Function<void()>& onDone) = 0;
        virtual void OnQueryValue(const infra::Function<void(int16_t)>& onResult) = 0;
        virtual void OnFail(const infra::Function<void(DemoError)>& onResult) = 0;
    };

    // Reference application category covering the shapes a consumer-defined category
    // needs: fire-and-forget command, payload command, query/response, category error.
    class DemoCategoryServer
        : public services::CanCategoryServer
        , public infra::Subject<DemoCategoryServerObserver>
    {
    public:
        explicit DemoCategoryServer(services::CanFrameTransport& transport);

        uint8_t Id() const override;

    private:
        void HandlePing(const hal::Can::Message& data);
        void HandleSetParameters(const hal::Can::Message& data);
        void HandleQueryValue(const hal::Can::Message& data);
        void HandleFail(const hal::Can::Message& data);

        void SendValueResponse(int16_t value);

        services::CanMessageHandler<DemoCategoryServer> ping{ demoPingId, *this, &DemoCategoryServer::HandlePing };
        services::CanMessageHandler<DemoCategoryServer> setParameters{ demoSetParametersId, *this, &DemoCategoryServer::HandleSetParameters };
        services::CanMessageHandler<DemoCategoryServer> queryValue{ demoQueryValueId, *this, &DemoCategoryServer::HandleQueryValue };
        services::CanMessageHandler<DemoCategoryServer> fail{ demoFailId, *this, &DemoCategoryServer::HandleFail };
    };

    class DemoCategoryClient;

    class DemoCategoryClientObserver
        : public infra::SingleObserver<DemoCategoryClientObserver, DemoCategoryClient>
    {
    public:
        using infra::SingleObserver<DemoCategoryClientObserver, DemoCategoryClient>::SingleObserver;

        virtual void OnValueResponse(int16_t value) = 0;
        virtual void OnCategoryError(uint8_t originatingCommandId, DemoError error) = 0;
    };

    class DemoCategoryClient
        : public services::CanCategoryClient
        , public infra::Subject<DemoCategoryClientObserver>
    {
    public:
        DemoCategoryClient(services::CanFrameTransport& transport, services::CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool SendPing(uint16_t targetNodeId);
        bool SendSetParameters(uint16_t targetNodeId, const DemoParameters& parameters);
        bool SendQueryValue(uint16_t targetNodeId);
        bool SendFail(uint16_t targetNodeId);

    private:
        void HandleValueResponse(const hal::Can::Message& data);
        void HandleCategoryError(const hal::Can::Message& data);

        services::CanMessageHandler<DemoCategoryClient> valueResponse{ demoValueResponseId, *this, &DemoCategoryClient::HandleValueResponse };
        services::CanMessageHandler<DemoCategoryClient> categoryError{ services::canCategoryErrorResponseMessageTypeId, *this, &DemoCategoryClient::HandleCategoryError };
    };

    class TestMessageType : public services::CanMessageType
    {
    public:
        explicit TestMessageType(uint8_t id);

        uint8_t Id() const override;
        void Handle(const hal::Can::Message&) override;

        int handleCount = 0;

    private:
        uint8_t msgId;
    };

    class SequencedTestCategory : public services::CanCategoryServer
    {
    public:
        SequencedTestCategory(services::CanFrameTransport& transport, uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        TestMessageType msg;

    private:
        uint8_t catId;
    };

    class SimpleTestCategory : public services::CanCategoryServer
    {
    public:
        SimpleTestCategory(services::CanFrameTransport& transport, uint8_t id);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

    private:
        uint8_t catId;
    };
}
