#pragma once

namespace services
{
    class FirmwareUpgradeHold
    {
    protected:
        FirmwareUpgradeHold() = default;
        FirmwareUpgradeHold(const FirmwareUpgradeHold& other) = delete;
        FirmwareUpgradeHold& operator=(const FirmwareUpgradeHold& other) = delete;
        ~FirmwareUpgradeHold() = default;

    public:
        virtual bool IsStoppedAndIdle() const = 0;
        virtual void Hold() = 0;
        virtual void Release() = 0;
    };
}
