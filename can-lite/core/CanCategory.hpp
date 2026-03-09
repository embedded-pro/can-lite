#pragma once

#include "can-lite/core/CanMessageType.hpp"
#include "infra/util/IntrusiveList.hpp"
#include <cstdint>

namespace services
{
    class CanCategory
    {
    public:
        virtual uint8_t Id() const = 0;
        virtual bool RequiresSequenceValidation() const = 0;

        void AddMessageType(CanMessageType& messageType);
        bool HandleMessage(uint8_t messageType, const hal::Can::Message& data);

    protected:
        CanCategory() = default;
        CanCategory(const CanCategory&) = delete;
        CanCategory& operator=(const CanCategory&) = delete;
        ~CanCategory() = default;

    private:
        infra::IntrusiveList<CanMessageType> messageTypes;
    };

    class CanCategoryServer
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryServer>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

    protected:
        CanCategoryServer() = default;
        ~CanCategoryServer() = default;
    };

    class CanCategoryClient
        : public CanCategory
        , public infra::IntrusiveList<CanCategoryClient>::NodeType
    {
    public:
        bool RequiresSequenceValidation() const override;

    protected:
        CanCategoryClient() = default;
        ~CanCategoryClient() = default;
    };
}
