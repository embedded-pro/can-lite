#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/testing/EchoCategoryDefinitions.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class EchoCategoryClient;

    class EchoCategoryClientObserver
        : public infra::SingleObserver<EchoCategoryClientObserver, EchoCategoryClient>
    {
    public:
        using infra::SingleObserver<EchoCategoryClientObserver, EchoCategoryClient>::SingleObserver;

        virtual void OnEchoReply(infra::ConstByteRange payload) = 0;
    };

    // The client half of the echo category. Commands go out sequenced, which is
    // what makes the pair useful for exercising the sequence contract end to end.
    class EchoCategoryClient
        : private CanCategoryHandlerStorage<1>
        , public CanCategoryClient
        , public infra::Subject<EchoCategoryClientObserver>
    {
    public:
        explicit EchoCategoryClient(uint8_t categoryId);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        bool SendEchoRequest(uint16_t targetNodeId, const hal::Can::Message& payload);
        bool SendValidatedRequest(uint16_t targetNodeId, const hal::Can::Message& payload);

        uint32_t ReplyCount() const;

    private:
        bool HandleEchoReply(infra::ConstByteRange payload);

        uint8_t categoryId;
        uint32_t replyCount = 0;
    };
}
