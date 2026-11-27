#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeControl.hpp"
#include "gmock/gmock.h"

namespace services
{
    class FirmwareUpgradeControlMock
        : public FirmwareUpgradeControl
    {
    public:
        MOCK_METHOD(bool, IsRunning, (), (const, override));
        MOCK_METHOD(void, ReadStagedVersion, (const infra::Function<void(FirmwareVersion)>& onRead), (override));
        MOCK_METHOD(void, CancelDownload, (), (override));
    };
}
