#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareVersion.hpp"
#include "infra/util/Function.hpp"

namespace services
{
    class FirmwareUpgradeControl
    {
    protected:
        FirmwareUpgradeControl() = default;
        FirmwareUpgradeControl(const FirmwareUpgradeControl& other) = delete;
        FirmwareUpgradeControl& operator=(const FirmwareUpgradeControl& other) = delete;
        ~FirmwareUpgradeControl() = default;

    public:
        virtual bool IsRunning() const = 0;
        virtual void ReadStagedVersion(const infra::Function<void(FirmwareVersion)>& onRead) = 0;
        virtual void CancelDownload() = 0;
    };
}
