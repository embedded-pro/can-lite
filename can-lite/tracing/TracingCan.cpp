#include "can-lite/tracing/TracingCan.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/stream/OutputStream.hpp"

namespace services
{
    namespace
    {
        constexpr const char* tracePrefix = "TracingCan: ";

        const char* PriorityName(CanPriority priority)
        {
            switch (priority)
            {
                case CanPriority::emergency:
                    return "emergency";
                case CanPriority::command:
                    return "command";
                case CanPriority::response:
                    return "response";
                case CanPriority::telemetry:
                    return "telemetry";
                case CanPriority::heartbeat:
                    return "heartbeat";
            }
            return "unknown";
        }

        const char* CategoryName(uint8_t category)
        {
            if (category == canSystemCategoryId)
                return "system";
            if (category == canFirmwareUpgradeCategoryId)
                return "firmwareUpgrade";
            return "application";
        }

        const char* MessageTypeName(uint8_t category, uint8_t messageType)
        {
            if (messageType == canCategoryErrorResponseMessageTypeId)
                return "categoryError";

            if (category != canSystemCategoryId)
                return "unknown";

            switch (messageType)
            {
                case canHeartbeatMessageTypeId:
                    return "heartbeat";
                case canCommandAckMessageTypeId:
                    return "commandAck";
                case canStatusRequestMessageTypeId:
                    return "statusRequest";
                case canCategoryListRequestMessageTypeId:
                    return "categoryListRequest";
                case canCategoryListResponseMessageTypeId:
                    return "categoryListResponse";
                default:
                    return "unknown";
            }
        }
    }

    TracingCan::TracingCan(hal::Can& can, Tracer& tracer)
        : can(can)
        , tracer(tracer)
    {}

    void TracingCan::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        TraceFrame("TX", id, data);

        can.SendData(id, data, actionOnCompletion);
    }

    void TracingCan::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        this->receivedAction = receivedAction;

        can.ReceiveData([this](Id id, const Message& data)
            {
                TraceFrame("RX", id, data);
                this->receivedAction(id, data);
            });
    }

    void TracingCan::TraceFrame(const char* direction, Id id, const Message& data)
    {
        if (id.Is29BitId())
            TraceExtendedFrame(direction, id.Get29BitId(), data);
        else
            tracer.Trace() << tracePrefix << direction << infra::hex << " standard id 0x" << id.Get11BitId()
                           << " dlc " << data.size() << " data " << infra::AsHex(infra::MakeRange(data));
    }

    void TracingCan::TraceExtendedFrame(const char* direction, uint32_t rawId, const Message& data)
    {
        auto category = ExtractCanCategory(rawId);
        auto messageType = ExtractCanMessageType(rawId);

        tracer.Trace() << tracePrefix << direction << infra::hex << " id 0x" << rawId
                       << " prio " << PriorityName(ExtractCanPriority(rawId))
                       << " cat 0x" << category << " " << CategoryName(category)
                       << " type 0x" << messageType << " " << MessageTypeName(category, messageType)
                       << " node 0x" << ExtractCanNodeId(rawId)
                       << " dlc " << data.size()
                       << " data " << infra::AsHex(infra::MakeRange(data));

        if (category == canSystemCategoryId && messageType == canCommandAckMessageTypeId && data.size() >= canCommandAckSize)
            TraceCommandAck(data);
    }

    void TracingCan::TraceCommandAck(const Message& data)
    {
        tracer.Trace() << tracePrefix << "ack" << infra::hex
                       << " cat 0x" << data[0]
                       << " type 0x" << data[1]
                       << " status " << CanAckStatusToString(static_cast<CanAckStatus>(data[2]))
                       << " expectedSeq 0x" << data[3];
    }
}
