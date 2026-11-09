#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeSession.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    FirmwareUpgradeSession::FirmwareUpgradeSession(FirmwareUpgradeCategoryServer& server, const std::optional<FirmwareStaging>& staging,
        FirmwareUpgradeHold& hold, const infra::Function<void()>& reset)
        : FirmwareUpgradeCategoryServerObserver(server)
        , staging(staging)
        , hold(hold)
        , reset(reset)
    {
        if (!this->staging)
            return;

        really_assert(this->staging->pageSize >= blockSize && this->staging->pageSize <= maxPageSize);
        journal.emplace(this->staging->journal);
        writer.emplace(this->staging->pack, this->staging->pageSize, infra::MakeRange(scratch), [this]()
            {
                return !TakeAbort();
            });
        reader.emplace(this->staging->pack, infra::MakeRange(scratch), [this]()
            {
                return !TakeAbort();
            });

        if (journal->Status() == application::UpgradePackStatus::downloaded)
        {
            state = State::staged;
            ReadStagedVersion();
        }
    }

    void FirmwareUpgradeSession::OnBeginUpgrade(uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult)
    {
        if (auto refusal = BeginRefusal(firmwareSize))
        {
            onResult(*refusal, 0);
            return;
        }

        beginDone = onResult;
        StartDownload(firmwareSize);
    }

    void FirmwareUpgradeSession::OnDataBlock(uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::receiving)
            onResult(FwuError::invalidState);
        else if (FlashBusy())
            onResult(FwuError::busy);
        else if (nextBlock != 0 && blockIndex == nextBlock - 1)
        {
            RestartDeadline();
            onResult(FwuError::ok);
        }
        else if (blockIndex != nextBlock)
            onResult(FwuError::sequenceError);
        else if (data.size() > packSize - writer->Written())
            onResult(FwuError::invalidSize);
        else
        {
            RestartDeadline();
            blockDone = onResult;
            ++nextBlock;
            writer->Append(data, [this](bool stored)
                {
                    OnBlockStored(stored);
                });
        }
    }

    void FirmwareUpgradeSession::OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::receiving)
            onResult(FwuError::invalidState);
        else if (FlashBusy())
            onResult(FwuError::busy);
        else if (writer->Written() != packSize)
            onResult(FwuError::invalidSize);
        else
        {
            deadline.Cancel();
            verifyDone = onResult;
            Enter(State::verifying);
            if (writer->HoldsPartialPage())
                writer->Flush([this, expectedCrc32](bool flushed)
                    {
                        if (flushed)
                            CheckCrc(expectedCrc32);
                        else
                            Fail(FwuError::writeError);
                    });
            else
                CheckCrc(expectedCrc32);
        }
    }

    void FirmwareUpgradeSession::OnActivate(const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::staged)
        {
            onResult(FwuError::notReady);
            return;
        }

        if (FlashBusy() || !hold.IsStoppedAndIdle())
        {
            onResult(FwuError::busy);
            return;
        }

        journal->Record(application::UpgradePackStatus::readyToDeploy, 0);
        Enter(State::restarting);
        onResult(FwuError::ok);
        restartTimer.Start(restartDelay, [this]()
            {
                reset();
            });
    }

    void FirmwareUpgradeSession::OnAbort(const infra::Function<void()>& onDone)
    {
        if (FlashBusy())
        {
            abortRequested = true;
            abortDone = onDone;
            return;
        }

        DoAbort();
        onDone();
    }

    void FirmwareUpgradeSession::OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult)
    {
        onResult(ReportedState(), nextBlock, TotalBlocks());
    }

    void FirmwareUpgradeSession::OnSessionTimeout()
    {
        OnDeadline();
    }

    bool FirmwareUpgradeSession::IsRunning() const
    {
        return Holds(state);
    }

    FirmwareVersion FirmwareUpgradeSession::StagedVersion() const
    {
        return stagedVersion;
    }

    void FirmwareUpgradeSession::CancelDownload()
    {
        if (!Downloading())
            return;

        if (FlashBusy())
            abortRequested = true;
        else
            EndDownload();
    }

    bool FirmwareUpgradeSession::Holds(State state)
    {
        return state == State::erasing || state == State::receiving || state == State::verifying || state == State::restarting;
    }

    void FirmwareUpgradeSession::Enter(State next)
    {
        const bool wasHeld = Holds(state);
        state = next;

        if (Holds(next) && !wasHeld)
            hold.Hold();
        else if (!Holds(next) && wasHeld)
            hold.Release();
    }

    bool FirmwareUpgradeSession::Downloading() const
    {
        return state == State::erasing || state == State::receiving || state == State::verifying;
    }

    std::optional<FwuError> FirmwareUpgradeSession::BeginRefusal(uint32_t firmwareSize) const
    {
        if (!staging)
            return FwuError::invalidSize;

        if (Downloading() || state == State::restarting || FlashBusy() || !hold.IsStoppedAndIdle())
            return FwuError::busy;

        if (firmwareSize < application::minimumUpgradePackSize || firmwareSize > staging->pack.TotalSize())
            return FwuError::invalidSize;

        return std::nullopt;
    }

    void FirmwareUpgradeSession::StartDownload(uint32_t firmwareSize)
    {
        discardTimer.Cancel();
        packSize = firmwareSize;
        nextBlock = 0;
        stagedVersion = {};
        Enter(State::erasing);

        if (journal->Status() != application::UpgradePackStatus::empty)
            journal->Clear();

        writer->Erase([this](bool erased)
            {
                OnSlotErased(erased);
            });
    }

    void FirmwareUpgradeSession::OnSlotErased(bool erased)
    {
        if (!erased)
        {
            Fail(FwuError::writeError);
            return;
        }

        Enter(State::receiving);
        RestartDeadline();
        beginDone(FwuError::ok, staging->pageSize);
    }

    void FirmwareUpgradeSession::OnBlockStored(bool stored)
    {
        if (stored)
            blockDone(FwuError::ok);
        else
            Fail(FwuError::writeError);
    }

    void FirmwareUpgradeSession::CheckCrc(uint32_t expectedCrc)
    {
        reader->CheckCrc(packSize, expectedCrc, [this](bool matches)
            {
                if (!matches)
                    Fail(FwuError::crcMismatch);
                else
                    reader->ReadHeader(packSize, [this](const application::StagedPackHeader& header)
                        {
                            OnVerifiedHeader(header);
                        });
            });
    }

    void FirmwareUpgradeSession::OnVerifiedHeader(const application::StagedPackHeader& header)
    {
        const auto version = application::CheckStagedPackHeader(header, application::StagedPackLimits{ staging->productName, applicationTargetName, staging->applicationCapacity, packSize });
        if (!version)
        {
            Fail(FwuError::notReady);
            return;
        }

        journal->Record(application::UpgradePackStatus::downloaded, 0);
        stagedVersion = *version;
        Enter(State::staged);
        verifyDone(FwuError::ok);
    }

    void FirmwareUpgradeSession::ReadStagedVersion()
    {
        reader->ReadHeader(staging->pack.TotalSize(), [this](const application::StagedPackHeader& header)
            {
                stagedVersion = application::ParseUpgradePackVersion(header.epilogue.productVersion).value_or(FirmwareVersion{});
                packSize = std::min<uint32_t>(header.epilogue.headerLength + header.image.lengthOfHeaderAndImage, staging->pack.TotalSize());
                nextBlock = TotalBlocks();
            });
    }

    bool FirmwareUpgradeSession::FlashBusy() const
    {
        return (writer && writer->Busy()) || (reader && reader->Busy());
    }

    bool FirmwareUpgradeSession::TakeAbort()
    {
        if (!abortRequested)
            return false;

        abortRequested = false;
        infra::AutoResetFunction<void()> done;
        done.Swap(abortDone);
        DoAbort();
        if (done != nullptr)
            done();
        return true;
    }

    void FirmwareUpgradeSession::DoAbort()
    {
        if (Downloading())
            EndDownload();
        else if (state == State::staged)
            Discard();
        else if (state == State::error)
            Enter(State::idle);
    }

    void FirmwareUpgradeSession::Fail(FwuError result)
    {
        deadline.Cancel();
        Enter(State::error);
        AnswerPending(result);
    }

    void FirmwareUpgradeSession::EndDownload()
    {
        deadline.Cancel();
        Enter(State::idle);
        AnswerPending(FwuError::invalidState);
    }

    void FirmwareUpgradeSession::AnswerPending(FwuError result)
    {
        if (beginDone != nullptr)
            beginDone(result, 0);
        if (blockDone != nullptr)
            blockDone(result);
        if (verifyDone != nullptr)
            verifyDone(result);
    }

    void FirmwareUpgradeSession::Discard()
    {
        Enter(State::idle);
        stagedVersion = {};
        TryDiscard();
    }

    void FirmwareUpgradeSession::TryDiscard()
    {
        if (!hold.IsStoppedAndIdle())
        {
            if (!discardTimer.Armed())
                discardTimer.Start(discardRetry, [this]()
                    {
                        TryDiscard();
                    });
            return;
        }

        discardTimer.Cancel();
        journal->Clear();
    }

    void FirmwareUpgradeSession::RestartDeadline()
    {
        deadline.Start(inactivityDeadline, [this]()
            {
                OnDeadline();
            });
    }

    void FirmwareUpgradeSession::OnDeadline()
    {
        if (state != State::receiving)
            return;

        if (FlashBusy())
            abortRequested = true;
        else
            EndDownload();
    }

    uint16_t FirmwareUpgradeSession::TotalBlocks() const
    {
        return static_cast<uint16_t>((packSize + blockSize - 1) / blockSize);
    }

    FwuState FirmwareUpgradeSession::ReportedState() const
    {
        switch (state)
        {
            case State::erasing:
            case State::receiving:
                return FwuState::receiving;
            case State::verifying:
                return FwuState::verifying;
            case State::staged:
            case State::restarting:
                return FwuState::complete;
            case State::error:
                return FwuState::error;
            default:
                return FwuState::idle;
        }
    }
}
