#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/testing/EchoCategoryDefinitions.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace services
{
    class EchoCategoryServer;

    class EchoCategoryServerObserver
        : public infra::SingleObserver<EchoCategoryServerObserver, EchoCategoryServer>
    {
    public:
        using infra::SingleObserver<EchoCategoryServerObserver, EchoCategoryServer>::SingleObserver;

        virtual void OnEchoRequest(infra::ConstByteRange payload) = 0;
        virtual void OnValidatedRequest(infra::ConstByteRange payload) = 0;
    };

    // The server half of the echo category. Its ID is a constructor parameter
    // because a consumer-owned category is assigned its ID by the integrator,
    // not by the library, and so is whether the link needs sequence validation.
    class EchoCategoryServer
        : private CanCategoryHandlerStorage<2>
        , public CanCategoryServer
        , public infra::Subject<EchoCategoryServerObserver>
    {
    public:
        explicit EchoCategoryServer(uint8_t categoryId, bool requiresSequenceValidation = true);

        uint8_t Id() const override;
        bool RequiresSequenceValidation() const override;

        uint32_t RequestCount() const;
        uint32_t ValidatedRequestCount() const;
        uint32_t RejectedRequestCount() const;

    private:
        bool HandleEchoRequest(infra::ConstByteRange payload);
        bool HandleValidatedRequest(infra::ConstByteRange payload);

        uint8_t categoryId;
        bool requiresSequenceValidation;
        uint32_t requestCount = 0;
        uint32_t validatedRequestCount = 0;
        uint32_t rejectedRequestCount = 0;
    };
}
