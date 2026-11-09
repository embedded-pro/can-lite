#include "bootloader/StatusJournal.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeSession.hpp"
#include "can-lite/categories/firmware_upgrade/test_doubles/TestUpgradePack.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousFlashStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/Crc.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>

namespace
{
    using application::UpgradePackStatus;
    using services::FwuError;
    using services::FwuState;
    using services::test::MakeTestPack;
    using PackDescription = services::test::TestPackDescription;
    using Pack = services::test::TestUpgradePack;
    using Events = infra::BoundedVector<infra::BoundedConstString>::WithMaxSize<4>;

    constexpr uint16_t pageSize{ 32 };
    constexpr uint32_t sectorSize{ 1024 };
    constexpr uint32_t numberOfSectors{ 4 };
    constexpr uint32_t applicationCapacity{ 2048 };
    constexpr const char* productName{ services::test::testPackProductName };

    class HoldFake
        : public services::FirmwareUpgradeHold
    {
    public:
        bool IsStoppedAndIdle() const override
        {
            return stoppedAndIdle;
        }

        void Hold() override
        {
            ++held;
        }

        void Release() override
        {
            --held;
        }

        bool stoppedAndIdle{ true };
        int held{ 0 };
    };

    class FailingEraseFlash
        : public hal::FlashStub
    {
    public:
        using hal::FlashStub::FlashStub;

        void EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone) override
        {
            if (failingSector == beginIndex)
                infra::EventDispatcher::Instance().Schedule(onDone);
            else
                hal::FlashStub::EraseSectors(beginIndex, endIndex, onDone);
        }

        std::optional<uint32_t> failingSector;
    };

    Events Expected(std::initializer_list<const char*> names)
    {
        Events events;
        for (const auto* name : names)
            events.push_back(name);
        return events;
    }

    uint32_t CrcOf(infra::ConstByteRange bytes)
    {
        infra::Crc32 crc;
        crc.Update(bytes);
        return crc.Result();
    }

    class FirmwareUpgradeSessionTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        FirmwareUpgradeSessionTest()
        {
            for (auto& sector : packFlash.sectors)
                std::ranges::fill(sector, 0x00);
        }

        void Emplace()
        {
            session.emplace(server, services::FirmwareStaging{ packFlash, journalFlash, productName, applicationCapacity, pageSize }, hold, [this]()
                {
                    reset.Call();
                });
        }

        void Construct()
        {
            Emplace();
            ExecuteAllActions();
        }

        uint8_t PackFlashByte(uint32_t offset) const
        {
            return packFlash.sectors[offset / sectorSize][offset % sectorSize];
        }

        bool PackFlashHolds(infra::ConstByteRange expected) const
        {
            for (uint32_t offset = 0; offset != expected.size(); ++offset)
                if (PackFlashByte(offset) != expected[offset])
                    return false;
            return true;
        }

        bool PackFlashErasedFrom(uint32_t offset) const
        {
            for (; offset != numberOfSectors * sectorSize; ++offset)
                if (PackFlashByte(offset) != 0xff)
                    return false;
            return true;
        }

        UpgradePackStatus JournalStatus()
        {
            return bootloader::StatusJournal{ journalFlash }.Status();
        }

        void RecordJournal(UpgradePackStatus status)
        {
            bootloader::StatusJournal{ journalFlash }.Record(status, 0);
        }

        void SendBegin(uint32_t size)
        {
            beginResult.reset();
            session->OnBeginUpgrade(size, [this](FwuError error, uint16_t page)
                {
                    events.push_back("begin");
                    beginResult.emplace(error, page);
                });
        }

        std::optional<std::pair<FwuError, uint16_t>> Begin(uint32_t size)
        {
            SendBegin(size);
            ExecuteAllActions();
            return beginResult;
        }

        void SendBlock(uint16_t index, infra::ConstByteRange data)
        {
            blockResult.reset();
            session->OnDataBlock(index, data, [this](FwuError error)
                {
                    blockResult = error;
                });
        }

        std::optional<FwuError> Block(uint16_t index, infra::ConstByteRange pack)
        {
            const auto offset = static_cast<std::size_t>(index) * services::FirmwareUpgradeSession::blockSize;
            const auto length = std::min<std::size_t>(services::FirmwareUpgradeSession::blockSize, pack.size() - offset);
            SendBlock(index, infra::Head(infra::DiscardHead(pack, offset), length));
            ExecuteAllActions();
            return blockResult;
        }

        static uint16_t NumberOfBlocks(infra::ConstByteRange pack)
        {
            return static_cast<uint16_t>((pack.size() + services::FirmwareUpgradeSession::blockSize - 1) / services::FirmwareUpgradeSession::blockSize);
        }

        void SendAllBlocks(infra::ConstByteRange pack)
        {
            for (uint16_t index = 0; index != NumberOfBlocks(pack); ++index)
                ASSERT_EQ(FwuError::ok, Block(index, pack)) << "block " << index;
        }

        void SendVerify(uint32_t crcValue)
        {
            verifyResult.reset();
            session->OnVerify(crcValue, [this](FwuError error)
                {
                    events.push_back("verify");
                    verifyResult = error;
                });
        }

        std::optional<FwuError> Verify(uint32_t crcValue)
        {
            SendVerify(crcValue);
            ExecuteAllActions();
            return verifyResult;
        }

        std::optional<FwuError> Download(infra::ConstByteRange pack)
        {
            EXPECT_EQ(FwuError::ok, Begin(static_cast<uint32_t>(pack.size()))->first);
            SendAllBlocks(pack);
            return Verify(CrcOf(pack));
        }

        std::optional<FwuError> Activate()
        {
            std::optional<FwuError> result;
            session->OnActivate([&result](FwuError error)
                {
                    result = error;
                });
            ExecuteAllActions();
            return result;
        }

        void SendAbort()
        {
            abortDone = false;
            session->OnAbort([this]()
                {
                    events.push_back("abort");
                    abortDone = true;
                });
        }

        bool Abort()
        {
            SendAbort();
            ExecuteAllActions();
            return abortDone;
        }

        std::tuple<FwuState, uint16_t, uint16_t> Progress()
        {
            std::tuple<FwuState, uint16_t, uint16_t> result{};
            session->OnQueryProgress([&result](FwuState state, uint16_t blocks, uint16_t total)
                {
                    result = { state, blocks, total };
                });
            return result;
        }

        testing::StrictMock<hal::CanMock> can;
        services::CanFrameTransport transport{ can, 1 };
        services::FirmwareUpgradeCategoryServer server{ transport };
        FailingEraseFlash packFlash{ numberOfSectors, sectorSize };
        hal::SynchronousFlashStub journalFlash{ 1, 512 };
        HoldFake hold;
        testing::StrictMock<testing::MockFunction<void()>> reset;
        std::optional<services::FirmwareUpgradeSession> session;

        Events events;
        std::optional<std::pair<FwuError, uint16_t>> beginResult;
        std::optional<FwuError> blockResult;
        std::optional<FwuError> verifyResult;
        bool abortDone{ false };
    };
}

TEST_F(FirmwareUpgradeSessionTest, begin_erases_the_journal_and_the_whole_slot_then_answers_the_page_size)
{
    RecordJournal(UpgradePackStatus::deployed);
    Construct();

    EXPECT_EQ(std::make_pair(FwuError::ok, pageSize), Begin(400));
    EXPECT_TRUE(PackFlashErasedFrom(0));
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_TRUE(session->IsRunning());
    EXPECT_EQ(1, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, begin_is_answered_only_after_the_last_sector_is_erased_one_sector_per_turn)
{
    Construct();

    SendBegin(400);
    EXPECT_EQ(0xff, packFlash.sectors[0][0]);
    EXPECT_EQ(0x00, packFlash.sectors[1][0]);

    while (packFlash.sectors[1][0] != 0xff)
        ExecuteFirstAction();
    EXPECT_EQ(0x00, packFlash.sectors[2][0]);
    EXPECT_FALSE(beginResult.has_value());

    ExecuteAllActions();
    EXPECT_EQ(std::make_pair(FwuError::ok, pageSize), beginResult);
}

TEST_F(FirmwareUpgradeSessionTest, begin_is_refused_busy_and_erases_nothing_while_the_application_runs)
{
    RecordJournal(UpgradePackStatus::downloaded);
    Construct();
    hold.stoppedAndIdle = false;

    EXPECT_EQ(FwuError::busy, Begin(400)->first);
    EXPECT_EQ(0x00, packFlash.sectors[0][0]);
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, begin_is_refused_invalid_size_for_a_pack_smaller_than_its_header_or_larger_than_the_slot)
{
    Construct();

    EXPECT_EQ(FwuError::invalidSize, Begin(application::minimumUpgradePackSize - 1)->first);
    EXPECT_EQ(FwuError::invalidSize, Begin(numberOfSectors * sectorSize + 1)->first);
    EXPECT_EQ(0x00, packFlash.sectors[0][0]);
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, begin_accepts_a_pack_of_exactly_the_slot_size)
{
    Construct();

    EXPECT_EQ(FwuError::ok, Begin(numberOfSectors * sectorSize)->first);
}

TEST_F(FirmwareUpgradeSessionTest, a_node_without_staging_answers_begin_invalid_size_verify_invalid_state_and_activate_not_ready)
{
    session.emplace(server, std::nullopt, hold, [this]()
        {
            reset.Call();
        });

    EXPECT_EQ(FwuError::invalidSize, Begin(400)->first);
    EXPECT_EQ(FwuError::invalidState, Verify(0));
    EXPECT_EQ(FwuError::notReady, Activate());
    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, a_second_begin_during_the_erase_or_the_download_is_answered_busy)
{
    Construct();

    SendBegin(400);
    std::optional<FwuError> second;
    session->OnBeginUpgrade(400, [&second](FwuError error, uint16_t)
        {
            second = error;
        });
    EXPECT_EQ(FwuError::busy, second);

    ExecuteAllActions();
    EXPECT_EQ(FwuError::ok, beginResult->first);
    EXPECT_EQ(FwuError::busy, Begin(400)->first);
    EXPECT_EQ(1, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, a_sector_that_does_not_erase_answers_begin_write_error_and_releases_the_application)
{
    Construct();
    packFlash.failingSector = 2;

    EXPECT_EQ(FwuError::writeError, Begin(400)->first);
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(FwuState::error, std::get<0>(Progress()));
}

TEST_F(FirmwareUpgradeSessionTest, blocks_are_programmed_into_the_staging_slot_page_by_page)
{
    Construct();
    const auto pack = MakeTestPack();

    EXPECT_EQ(FwuError::ok, Begin(static_cast<uint32_t>(pack.size()))->first);
    SendAllBlocks(infra::MakeRange(pack));

    const auto fullPages = static_cast<uint32_t>(pack.size() / pageSize * pageSize);
    EXPECT_TRUE(PackFlashHolds(infra::Head(infra::MakeRange(pack), fullPages)));
    EXPECT_TRUE(PackFlashErasedFrom(fullPages));
}

TEST_F(FirmwareUpgradeSessionTest, a_block_that_fills_a_page_is_acknowledged_once_the_page_is_programmed)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));

    for (uint16_t index = 0; index != 5; ++index)
        EXPECT_EQ(FwuError::ok, Block(index, infra::MakeRange(pack)));

    SendBlock(5, infra::ConstByteRange{ pack.data() + 30, pack.data() + 36 });
    EXPECT_FALSE(blockResult.has_value());
    EXPECT_EQ(FwuError::busy, [this]()
        {
            std::optional<FwuError> result;
            session->OnDataBlock(6, infra::ConstByteRange{}, [&result](FwuError error)
                {
                    result = error;
                });
            return result;
        }());

    ExecuteAllActions();
    EXPECT_EQ(FwuError::ok, blockResult);
    EXPECT_TRUE(PackFlashHolds(infra::Head(infra::MakeRange(pack), pageSize)));
}

TEST_F(FirmwareUpgradeSessionTest, a_repeat_of_the_last_acknowledged_block_is_acknowledged_again_without_being_written)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));

    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));
    EXPECT_EQ(FwuError::ok, Block(1, infra::MakeRange(pack)));
    EXPECT_EQ(2, std::get<1>(Progress()));
}

TEST_F(FirmwareUpgradeSessionTest, a_block_out_of_order_is_answered_sequence_error_and_written_nowhere)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));

    EXPECT_EQ(FwuError::sequenceError, Block(2, infra::MakeRange(pack)));
    EXPECT_EQ(1, std::get<1>(Progress()));
    EXPECT_EQ(FwuError::ok, Block(1, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, a_block_past_the_size_given_in_begin_is_answered_invalid_size)
{
    Construct();
    const Pack bytes(application::minimumUpgradePackSize + 2, 0x11);
    Begin(application::minimumUpgradePackSize);

    for (uint16_t index = 0; index * 6u + 6u <= application::minimumUpgradePackSize; ++index)
        ASSERT_EQ(FwuError::ok, Block(index, infra::MakeRange(bytes)));

    SendBlock(static_cast<uint16_t>(application::minimumUpgradePackSize / 6), infra::ConstByteRange{ bytes.data(), bytes.data() + 6 });
    ExecuteAllActions();
    EXPECT_EQ(FwuError::invalidSize, blockResult);
}

TEST_F(FirmwareUpgradeSessionTest, a_block_outside_a_download_is_answered_invalid_state)
{
    Construct();
    const auto pack = MakeTestPack();

    EXPECT_EQ(FwuError::invalidState, Block(0, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, a_page_that_does_not_read_back_as_written_is_answered_write_error)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    packFlash.stopAfterWriteSteps = 0;

    for (uint16_t index = 0; index != 5; ++index)
        EXPECT_EQ(FwuError::ok, Block(index, infra::MakeRange(pack)));
    EXPECT_EQ(FwuError::writeError, Block(5, infra::MakeRange(pack)));

    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(FwuError::invalidState, Block(6, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, verify_stages_a_checked_pack_records_downloaded_and_releases_the_application)
{
    Construct();
    const auto pack = MakeTestPack();

    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(pack)));
    EXPECT_TRUE(PackFlashHolds(infra::MakeRange(pack)));
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());
    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), session->StagedVersion());
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(FwuState::complete, std::get<0>(Progress()));
}

TEST_F(FirmwareUpgradeSessionTest, verify_with_another_crc_answers_crc_mismatch_and_stages_nothing)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    SendAllBlocks(infra::MakeRange(pack));

    EXPECT_EQ(FwuError::crcMismatch, Verify(CrcOf(infra::MakeRange(pack)) ^ 1));
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(FwuState::error, std::get<0>(Progress()));
    EXPECT_EQ(FwuError::notReady, Activate());
}

TEST_F(FirmwareUpgradeSessionTest, verify_before_every_byte_arrived_is_answered_invalid_size_and_the_download_continues)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));

    EXPECT_EQ(FwuError::invalidSize, Verify(CrcOf(infra::MakeRange(pack))));
    EXPECT_TRUE(session->IsRunning());
    EXPECT_EQ(FwuError::ok, Block(1, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, verify_outside_a_download_is_answered_invalid_state)
{
    Construct();

    EXPECT_EQ(FwuError::invalidState, Verify(0));
}

TEST_F(FirmwareUpgradeSessionTest, a_pack_whose_header_fails_the_check_answers_verify_not_ready_and_stages_nothing)
{
    Construct();
    PackDescription description;
    description.productName = infra::BoundedConstString{ "another product" };

    EXPECT_EQ(FwuError::notReady, Download(infra::MakeRange(MakeTestPack(description))));
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, a_staged_pack_stays_staged_across_a_restart)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    session.reset();
    Construct();

    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), session->StagedVersion());
    EXPECT_EQ(FwuState::complete, std::get<0>(Progress()));
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());
}

TEST_F(FirmwareUpgradeSessionTest, begin_while_the_staged_header_is_still_read_at_start_up_is_answered_busy)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));
    session.reset();
    Emplace();

    std::optional<FwuError> result;
    session->OnBeginUpgrade(400, [&result](FwuError error, uint16_t)
        {
            result = error;
        });
    EXPECT_EQ(FwuError::busy, result);
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());

    ExecuteAllActions();
    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), session->StagedVersion());
    EXPECT_EQ(FwuError::ok, Begin(400)->first);
}

TEST_F(FirmwareUpgradeSessionTest, query_progress_after_a_restart_reports_the_staged_pack_complete)
{
    Construct();
    const auto pack = MakeTestPack();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(pack)));
    session.reset();
    Construct();

    const auto blocks = NumberOfBlocks(infra::MakeRange(pack));
    EXPECT_EQ(std::make_tuple(FwuState::complete, blocks, blocks), Progress());
}

TEST_F(FirmwareUpgradeSessionTest, a_journal_that_reads_anything_but_downloaded_stages_nothing_at_start_up)
{
    RecordJournal(UpgradePackStatus::readyToDeploy);
    Construct();

    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
    EXPECT_EQ(FwuError::notReady, Activate());
}

TEST_F(FirmwareUpgradeSessionTest, activate_without_a_staged_pack_is_answered_not_ready)
{
    Construct();

    EXPECT_EQ(FwuError::notReady, Activate());
}

TEST_F(FirmwareUpgradeSessionTest, activate_while_the_application_runs_is_answered_busy_and_keeps_the_pack_staged)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));
    hold.stoppedAndIdle = false;

    EXPECT_EQ(FwuError::busy, Activate());
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(FirmwareUpgradeSessionTest, activate_records_ready_to_deploy_answers_and_resets_after_the_restart_delay)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    EXPECT_EQ(FwuError::ok, Activate());
    EXPECT_EQ(UpgradePackStatus::readyToDeploy, JournalStatus());
    EXPECT_TRUE(session->IsRunning());
    EXPECT_EQ(1, hold.held);

    ForwardTime(services::FirmwareUpgradeSession::restartDelay - std::chrono::milliseconds(1));
    EXPECT_CALL(reset, Call());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(FirmwareUpgradeSessionTest, begin_while_restarting_is_answered_busy)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));
    EXPECT_EQ(FwuError::ok, Activate());

    EXPECT_EQ(FwuError::busy, Begin(400)->first);
    EXPECT_CALL(reset, Call());
    ForwardTime(services::FirmwareUpgradeSession::restartDelay);
}

TEST_F(FirmwareUpgradeSessionTest, abort_during_a_download_returns_to_idle_with_the_journal_empty)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));

    EXPECT_TRUE(Abort());
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(FwuState::idle, std::get<0>(Progress()));
    EXPECT_EQ(FwuError::invalidState, Block(1, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, abort_during_the_erase_stops_after_the_sector_and_answers_begin_invalid_state_first)
{
    Construct();

    SendBegin(400);
    SendAbort();
    ExecuteAllActions();

    EXPECT_EQ(Expected({ "begin", "abort" }), events);
    EXPECT_EQ(FwuError::invalidState, beginResult->first);
    EXPECT_EQ(0x00, packFlash.sectors[1][0]);
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, abort_while_a_page_is_programmed_finishes_the_page_then_answers_the_block_invalid_state_first)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    for (uint16_t index = 0; index != 5; ++index)
        EXPECT_EQ(FwuError::ok, Block(index, infra::MakeRange(pack)));

    Events order;
    std::optional<FwuError> lastBlock;
    session->OnDataBlock(5, infra::ConstByteRange{ pack.data() + 30, pack.data() + 36 }, [&order, &lastBlock](FwuError error)
        {
            order.push_back("block");
            lastBlock = error;
        });
    session->OnAbort([&order]()
        {
            order.push_back("abort");
        });
    EXPECT_TRUE(order.empty());

    ExecuteAllActions();
    EXPECT_EQ(Expected({ "block", "abort" }), order);
    EXPECT_EQ(FwuError::invalidState, lastBlock);
    EXPECT_TRUE(PackFlashHolds(infra::Head(infra::MakeRange(pack), pageSize)));
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
}

TEST_F(FirmwareUpgradeSessionTest, abort_while_restarting_changes_nothing_and_the_reset_follows)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));
    EXPECT_EQ(FwuError::ok, Activate());

    EXPECT_TRUE(Abort());
    EXPECT_TRUE(session->IsRunning());
    EXPECT_EQ(1, hold.held);
    EXPECT_EQ(UpgradePackStatus::readyToDeploy, JournalStatus());
    EXPECT_EQ(FwuState::complete, std::get<0>(Progress()));

    EXPECT_CALL(reset, Call());
    ForwardTime(services::FirmwareUpgradeSession::restartDelay);
}

TEST_F(FirmwareUpgradeSessionTest, abort_during_verify_answers_verify_invalid_state_first)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    SendAllBlocks(infra::MakeRange(pack));

    SendVerify(CrcOf(infra::MakeRange(pack)));
    SendAbort();
    ExecuteAllActions();

    EXPECT_EQ(Expected({ "begin", "verify", "abort" }), events);
    EXPECT_EQ(FwuError::invalidState, verifyResult);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, abort_of_a_staged_pack_discards_it_at_once_and_erases_the_journal_once_the_application_is_stopped)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));
    hold.stoppedAndIdle = false;

    EXPECT_TRUE(Abort());
    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
    EXPECT_EQ(FwuError::notReady, Activate());
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_EQ(UpgradePackStatus::downloaded, JournalStatus());

    hold.stoppedAndIdle = true;
    ForwardTime(services::FirmwareUpgradeSession::discardRetry);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
}

TEST_F(FirmwareUpgradeSessionTest, abort_of_a_staged_pack_while_the_application_is_stopped_erases_the_journal_at_once)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    EXPECT_TRUE(Abort());
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
}

TEST_F(FirmwareUpgradeSessionTest, abort_after_a_failed_verify_returns_to_idle)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    SendAllBlocks(infra::MakeRange(pack));
    Verify(0);

    EXPECT_TRUE(Abort());
    EXPECT_EQ(FwuState::idle, std::get<0>(Progress()));
}

TEST_F(FirmwareUpgradeSessionTest, the_download_ends_30_s_after_the_last_accepted_command)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));

    ForwardTime(services::FirmwareUpgradeSession::inactivityDeadline - std::chrono::seconds(1));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));
    ForwardTime(services::FirmwareUpgradeSession::inactivityDeadline - std::chrono::seconds(1));
    EXPECT_TRUE(session->IsRunning());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(FwuError::invalidState, Block(1, infra::MakeRange(pack)));
}

TEST_F(FirmwareUpgradeSessionTest, no_deadline_runs_for_a_staged_pack)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    ForwardTime(std::chrono::minutes(5));
    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), session->StagedVersion());
}

TEST_F(FirmwareUpgradeSessionTest, the_session_timeout_of_the_category_ends_a_download)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));

    session->OnSessionTimeout();
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, cancel_download_ends_a_download_without_an_abort_answer)
{
    Construct();
    const auto pack = MakeTestPack();
    Begin(static_cast<uint32_t>(pack.size()));
    EXPECT_EQ(FwuError::ok, Block(0, infra::MakeRange(pack)));

    session->CancelDownload();
    ExecuteAllActions();

    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(FwuError::invalidState, Block(1, infra::MakeRange(pack)));
    EXPECT_EQ(Expected({ "begin" }), events);
}

TEST_F(FirmwareUpgradeSessionTest, cancel_download_during_the_erase_answers_begin_invalid_state)
{
    Construct();

    SendBegin(400);
    session->CancelDownload();
    ExecuteAllActions();

    EXPECT_EQ(FwuError::invalidState, beginResult->first);
    EXPECT_FALSE(session->IsRunning());
    EXPECT_EQ(0, hold.held);
}

TEST_F(FirmwareUpgradeSessionTest, cancel_download_keeps_a_staged_pack_and_an_accepted_activate)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    session->CancelDownload();
    EXPECT_EQ((services::FirmwareVersion{ 2, 3, 4 }), session->StagedVersion());

    EXPECT_EQ(FwuError::ok, Activate());
    session->CancelDownload();
    EXPECT_CALL(reset, Call());
    ForwardTime(services::FirmwareUpgradeSession::restartDelay);
}

TEST_F(FirmwareUpgradeSessionTest, query_progress_reports_the_blocks_received_and_the_blocks_of_the_pack)
{
    Construct();
    const auto pack = MakeTestPack();
    EXPECT_EQ(FwuState::idle, std::get<0>(Progress()));

    Begin(static_cast<uint32_t>(pack.size()));
    Block(0, infra::MakeRange(pack));
    Block(1, infra::MakeRange(pack));

    EXPECT_EQ(std::make_tuple(FwuState::receiving, uint16_t{ 2 }, NumberOfBlocks(infra::MakeRange(pack))), Progress());
}

TEST_F(FirmwareUpgradeSessionTest, a_new_begin_discards_a_staged_pack)
{
    Construct();
    EXPECT_EQ(FwuError::ok, Download(infra::MakeRange(MakeTestPack())));

    EXPECT_EQ(FwuError::ok, Begin(400)->first);
    EXPECT_EQ(UpgradePackStatus::empty, JournalStatus());
    EXPECT_EQ(services::FirmwareVersion{}, session->StagedVersion());
}
