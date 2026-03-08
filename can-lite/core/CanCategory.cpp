#include "can-lite/core/CanCategory.hpp"

namespace services
{
    bool CanCategory::RequiresSequenceValidation() const
    {
        return true;
    }

    void CanCategory::AddMessageType(CanMessageType& messageType)
    {
        messageTypes.push_back(messageType);
    }

    bool CanCategory::HandleMessage(uint8_t messageType, const hal::Can::Message& data)
    {
        for (auto& handler : messageTypes)
        {
            if (handler.Id() == messageType)
            {
                handler.Handle(data);
                return true;
            }
        }

        return false;
    }
}
