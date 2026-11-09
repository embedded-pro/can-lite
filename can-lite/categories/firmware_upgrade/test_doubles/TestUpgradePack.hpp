#pragma once

#include "infra/util/BoundedString.hpp"
#include "infra/util/BoundedVector.hpp"
#include "upgrade/staging/UpgradePackCheck.hpp"
#include <cstdint>

namespace services::test
{
    inline constexpr const char* testPackProductName = "can-lite test board";
    inline constexpr uint32_t testPackSignatureLength = 64;
    inline constexpr uint32_t testPackAesCounterSize = 16;
    inline constexpr std::size_t maxTestPackSize = 4096;

    using TestUpgradePack = infra::BoundedVector<uint8_t>::WithMaxSize<maxTestPackSize>;

    struct TestPackDescription
    {
        infra::BoundedConstString productName{ testPackProductName };
        infra::BoundedConstString productVersion{ "2.3.4" };
        infra::BoundedConstString target{ "app" };
        uint32_t imageSize{ 300 };
    };

    TestUpgradePack MakeTestPack(const TestPackDescription& description = {});
}
