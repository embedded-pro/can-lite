#pragma once

#include "bootloader/StatusJournal.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareStaging.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeControl.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeHold.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "upgrade/staging/PackStagingWriter.hpp"
#include "upgrade/staging/StagedPackReader.hpp"
#include "upgrade/staging/UpgradePackCheck.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace services
{
    class FirmwareUpgradeSession
        : public FirmwareUpgradeCategoryServerObserver
        , public FirmwareUpgradeControl
    {
    public:
        static constexpr uint16_t maxPageSize{ application::PackStagingWriter::maxPageSize };
        static constexpr uint8_t blockSize{ 6 };
        static constexpr const char* applicationTargetName{ "app" };
        static constexpr infra::Duration inactivityDeadline{ std::chrono::seconds(30) };
        static constexpr infra::Duration restartDelay{ std::chrono::milliseconds(250) };
        static constexpr infra::Duration discardRetry{ std::chrono::milliseconds(100) };

        FirmwareUpgradeSession(FirmwareUpgradeCategoryServer& server, const std::optional<FirmwareStaging>& staging,
            FirmwareUpgradeHold& hold, const infra::Function<void()>& reset);

        void OnBeginUpgrade(uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult) override;
        void OnDataBlock(uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult) override;
        void OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult) override;
        void OnActivate(const infra::Function<void(FwuError)>& onResult) override;
        void OnAbort(const infra::Function<void()>& onDone) override;
        void OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult) override;
        void OnSessionTimeout() override;

        bool IsRunning() const override;
        FirmwareVersion StagedVersion() const override;
        void CancelDownload() override;

    private:
        enum class State : uint8_t
        {
            idle,
            erasing,
            receiving,
            verifying,
            staged,
            error,
            restarting
        };

        static bool Holds(State state);
        void Enter(State next);
        bool Downloading() const;
        std::optional<FwuError> BeginRefusal(uint32_t firmwareSize) const;
        void StartDownload(uint32_t firmwareSize);
        void OnSlotErased(bool erased);
        void OnBlockStored(bool stored);
        void CheckCrc(uint32_t expectedCrc);
        void OnVerifiedHeader(const application::StagedPackHeader& header);
        void ReadStagedVersion();

        bool FlashBusy() const;
        bool TakeAbort();
        void DoAbort();
        void Fail(FwuError result);
        void EndDownload();
        void AnswerPending(FwuError result);
        void Discard();
        void TryDiscard();
        void RestartDeadline();
        void OnDeadline();
        FwuState ReportedState() const;
        uint16_t TotalBlocks() const;

        std::optional<FirmwareStaging> staging;
        FirmwareUpgradeHold& hold;
        infra::Function<void()> reset;
        std::optional<bootloader::StatusJournal> journal;
        std::array<uint8_t, maxPageSize> scratch{};
        std::optional<application::PackStagingWriter> writer;
        std::optional<application::StagedPackReader> reader;

        State state{ State::idle };
        bool abortRequested{ false };
        infra::AutoResetFunction<void()> abortDone;

        uint32_t packSize{ 0 };
        uint16_t nextBlock{ 0 };
        FirmwareVersion stagedVersion{};

        infra::AutoResetFunction<void(FwuError, uint16_t)> beginDone;
        infra::AutoResetFunction<void(FwuError)> blockDone;
        infra::AutoResetFunction<void(FwuError)> verifyDone;

        infra::TimerSingleShot deadline;
        infra::TimerSingleShot restartTimer;
        infra::TimerRepeating discardTimer;
    };
}
