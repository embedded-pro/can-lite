#include "support/TestCategories.hpp"
#include "can-lite/core/CanPayload.hpp"

namespace integration
{
    DemoCategoryServer::DemoCategoryServer(services::CanFrameTransport& transport)
        : services::CanCategoryServer(transport)
    {
        AddMessageTypes(ping, setParameters, queryValue, fail);
    }

    uint8_t DemoCategoryServer::Id() const
    {
        return demoCategoryId;
    }

    void DemoCategoryServer::HandlePing(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnPing([&server]()
                    {
                        server.SendCommandAck(demoPingId, services::CanAckStatus::success);
                    });
            });
    }

    void DemoCategoryServer::HandleSetParameters(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        reader.Skip(1);
        DemoParameters parameters{ reader.ReadInt16(), reader.ReadInt16(), reader.ReadInt16() };

        if (!reader.Valid())
        {
            SendCommandAck(demoSetParametersId, services::CanAckStatus::invalidPayload);
            return;
        }

        auto& server = *this;
        NotifyObservers([&server, parameters](auto& observer)
            {
                observer.OnSetParameters(parameters, [&server]()
                    {
                        server.SendCommandAck(demoSetParametersId, services::CanAckStatus::success);
                    });
            });
    }

    void DemoCategoryServer::HandleQueryValue(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnQueryValue([&server](int16_t value)
                    {
                        server.SendValueResponse(value);
                        server.SendCommandAck(demoQueryValueId, services::CanAckStatus::success);
                    });
            });
    }

    void DemoCategoryServer::HandleFail(const hal::Can::Message&)
    {
        auto& server = *this;
        NotifyObservers([&server](auto& observer)
            {
                observer.OnFail([&server](DemoError error)
                    {
                        server.SendCategoryError(demoFailId, static_cast<uint8_t>(error));
                        server.SendCommandAck(demoFailId, services::CanAckStatus::categoryError);
                    });
            });
    }

    void DemoCategoryServer::SendValueResponse(int16_t value)
    {
        services::CanPayloadWriter payload;
        payload.WriteInt16(value);

        SendResponse(demoValueResponseId, payload);
    }

    DemoCategoryClient::DemoCategoryClient(services::CanFrameTransport& transport, services::CanSequenceSource& sequenceSource)
        : services::CanCategoryClient(transport, sequenceSource)
    {
        AddMessageTypes(valueResponse, categoryError);
    }

    uint8_t DemoCategoryClient::Id() const
    {
        return demoCategoryId;
    }

    bool DemoCategoryClient::SendPing(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, demoPingId);
    }

    bool DemoCategoryClient::SendSetParameters(uint16_t targetNodeId, const DemoParameters& parameters)
    {
        services::CanPayloadWriter payload;
        payload.WriteInt16(parameters.first).WriteInt16(parameters.second).WriteInt16(parameters.third);

        return SendCommand(targetNodeId, demoSetParametersId, payload);
    }

    bool DemoCategoryClient::SendQueryValue(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, demoQueryValueId);
    }

    bool DemoCategoryClient::SendFail(uint16_t targetNodeId)
    {
        return SendCommand(targetNodeId, demoFailId);
    }

    void DemoCategoryClient::HandleValueResponse(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        auto value = reader.ReadInt16();

        if (!reader.Valid())
            return;

        NotifyObservers([value](auto& observer)
            {
                observer.OnValueResponse(value);
            });
    }

    void DemoCategoryClient::HandleCategoryError(const hal::Can::Message& data)
    {
        services::CanPayloadReader reader{ data };
        auto originatingCommandId = reader.ReadUInt8();
        auto error = static_cast<DemoError>(reader.ReadUInt8());

        if (!reader.Valid())
            return;

        NotifyObservers([originatingCommandId, error](auto& observer)
            {
                observer.OnCategoryError(originatingCommandId, error);
            });
    }

    TestMessageType::TestMessageType(uint8_t id)
        : msgId(id)
    {}

    uint8_t TestMessageType::Id() const
    {
        return msgId;
    }

    void TestMessageType::Handle(const hal::Can::Message&)
    {
        handleCount++;
    }

    SequencedTestCategory::SequencedTestCategory(services::CanFrameTransport& transport, uint8_t id)
        : services::CanCategoryServer(transport)
        , msg(0x01)
        , catId(id)
    {
        AddMessageType(msg);
    }

    uint8_t SequencedTestCategory::Id() const
    {
        return catId;
    }

    bool SequencedTestCategory::RequiresSequenceValidation() const
    {
        return true;
    }

    SimpleTestCategory::SimpleTestCategory(services::CanFrameTransport& transport, uint8_t id)
        : services::CanCategoryServer(transport)
        , catId(id)
    {}

    uint8_t SimpleTestCategory::Id() const
    {
        return catId;
    }

    bool SimpleTestCategory::RequiresSequenceValidation() const
    {
        return false;
    }
}
