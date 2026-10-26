#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanCommandAcknowledgerMock.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>

namespace
{
    using namespace services;
    using testing::_;
    using testing::ElementsAreArray;
    using testing::Invoke;
    using testing::SaveArg;
    using testing::StrictMock;

    class FirmwareUpgradeCategoryServerObserverMock
        : public FirmwareUpgradeCategoryServerObserver
    {
    public:
        using FirmwareUpgradeCategoryServerObserver::FirmwareUpgradeCategoryServerObserver;

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnVerify, (uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnActivate, (const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnAbort, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnQueryProgress, (const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnSessionTimeout, (), (override));
    };

    class TestFirmwareUpgradeCategoryServer
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestFirmwareUpgradeCategoryServer()
        {
            server.SetAcknowledger(acknowledger);
        }

        void ExpectResponse(uint8_t messageType, std::initializer_list<uint8_t> bytes, uint16_t nodeId = 1)
        {
            EXPECT_CALL(canMock, SendData(
                                     hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, firmwareUpgradeCategoryId, messageType, nodeId)),
                                     ElementsAreArray(bytes),
                                     testing::_))
                .WillOnce([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    });
        }

        void ExpectAck(uint8_t commandId, CanAckStatus status = CanAckStatus::success)
        {
            EXPECT_CALL(acknowledger, SendCommandAck(firmwareUpgradeCategoryId, commandId, status));
        }

        static hal::Can::Message BeginUpgradeMessage()
        {
            hal::Can::Message data;
            data.resize(4, 0);
            CanFrameCodec::WriteInt32(data, 0, 6);
            return data;
        }

        static hal::Can::Message DataBlockMessage(uint16_t blockIndex)
        {
            hal::Can::Message data;
            data.resize(8, 0);
            CanFrameCodec::WriteUInt16(data, 0, blockIndex);
            return data;
        }

        static hal::Can::Message VerifyMessage()
        {
            hal::Can::Message data;
            data.resize(4, 0);
            return data;
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FirmwareUpgradeCategoryServer::Config config{ std::chrono::seconds(30) };
        FirmwareUpgradeCategoryServer server{ transport, config };
        StrictMock<CanCommandAcknowledgerMock> acknowledger;
    };

    class TestFirmwareUpgradeCategoryServerWithObserver : public TestFirmwareUpgradeCategoryServer
    {
    public:
        void AcceptBeginUpgrade()
        {
            EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
                {
                    cb(FwuError::ok, 4096);
                }));
            ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
            ExpectAck(fwuBeginUpgradeId);

            server.HandleMessage(fwuBeginUpgradeId, BeginUpgradeMessage());
        }

        StrictMock<FirmwareUpgradeCategoryServerObserverMock> observer{ server };
    };

    TEST_F(TestFirmwareUpgradeCategoryServer, Id)
    {
        EXPECT_EQ(server.Id(), firmwareUpgradeCategoryId);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, RequiresSequenceValidation_ReturnsFalse)
    {
        EXPECT_FALSE(server.RequiresSequenceValidation());
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, BeginUpgrade_CallbackSendsResponseAndAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(12288u, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnSessionTimeout()).Times(0);
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 12288);
        server.HandleMessage(fwuBeginUpgradeId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, BeginUpgrade_ObserverReportsFailure_AcksCategoryError)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::busy, 0);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::busy), 0x00, 0x00 });
        ExpectAck(fwuBeginUpgradeId, CanAckStatus::categoryError);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 12288);
        server.HandleMessage(fwuBeginUpgradeId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, BeginUpgrade_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        EXPECT_EQ(server.HandleMessage(fwuBeginUpgradeId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, DataBlock_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnDataBlock(42, _, _)).WillOnce(Invoke([](uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& cb)
            {
                EXPECT_EQ(blockIndex, 42);
                EXPECT_EQ(data, (std::array<uint8_t, 6>{ 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x00 }));
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::ok), 0x00, 0x2A });
        ExpectAck(fwuDataBlockId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message block;
        block.resize(8, 0);
        CanFrameCodec::WriteInt16(block, 0, 42);
        block[2] = 0xAA;
        block[3] = 0xBB;
        server.HandleMessage(fwuDataBlockId, block);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, DataBlock_ObserverReportsFailure_AcksCategoryError)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnDataBlock(_, _, _)).WillOnce(Invoke([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::writeError);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::writeError), 0x00, 0x00 });
        ExpectAck(fwuDataBlockId, CanAckStatus::categoryError);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message block;
        block.resize(8, 0);
        server.HandleMessage(fwuDataBlockId, block);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, DataBlock_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        EXPECT_EQ(server.HandleMessage(fwuDataBlockId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Verify_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnVerify(0xABCD1234u, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::ok) });
        ExpectAck(fwuVerifyId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, static_cast<int32_t>(0xABCD1234u));
        server.HandleMessage(fwuVerifyId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Verify_ObserverReportsFailure_AcksCategoryError)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnVerify(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::crcMismatch);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::crcMismatch) });
        ExpectAck(fwuVerifyId, CanAckStatus::categoryError);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        data.resize(4, 0);
        server.HandleMessage(fwuVerifyId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Verify_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        EXPECT_EQ(server.HandleMessage(fwuVerifyId, data), CanDispatchResult::rejected);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Activate_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnActivate(_)).WillOnce(Invoke([](const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuActivateResponseId, { static_cast<uint8_t>(FwuError::ok) });
        ExpectAck(fwuActivateId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuActivateId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Activate_ObserverReportsFailure_AcksCategoryError)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnActivate(_)).WillOnce(Invoke([](const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::sequenceError);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuActivateResponseId, { static_cast<uint8_t>(FwuError::sequenceError) });
        ExpectAck(fwuActivateId, CanAckStatus::categoryError);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuActivateId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Abort_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnAbort(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectAck(fwuAbortId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuAbortId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, QueryProgress_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnQueryProgress(_)).WillOnce(Invoke([](const infra::Function<void(FwuState, uint16_t, uint16_t)>& cb)
            {
                cb(FwuState::receiving, 100, 2048);
            }));
        ExpectResponse(fwuProgressResponseId, { static_cast<uint8_t>(FwuState::receiving), 0x00, 0x64, 0x08, 0x00 });
        ExpectAck(fwuQueryProgressId);

        hal::Can::Message data;
        server.HandleMessage(fwuQueryProgressId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, BeginUpgrade_WithoutObserver_AnswersNotReady)
    {
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::notReady), 0x00, 0x00 });
        ExpectAck(fwuBeginUpgradeId, CanAckStatus::categoryError);

        EXPECT_EQ(server.HandleMessage(fwuBeginUpgradeId, BeginUpgradeMessage()), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, DataBlock_WithoutObserver_AnswersNotReady)
    {
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::notReady), 0x00, 0x2A });
        ExpectAck(fwuDataBlockId, CanAckStatus::categoryError);

        EXPECT_EQ(server.HandleMessage(fwuDataBlockId, DataBlockMessage(42)), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Verify_WithoutObserver_AnswersNotReady)
    {
        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::notReady) });
        ExpectAck(fwuVerifyId, CanAckStatus::categoryError);

        EXPECT_EQ(server.HandleMessage(fwuVerifyId, VerifyMessage()), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Activate_WithoutObserver_AnswersNotReady)
    {
        ExpectResponse(fwuActivateResponseId, { static_cast<uint8_t>(FwuError::notReady) });
        ExpectAck(fwuActivateId, CanAckStatus::categoryError);

        EXPECT_EQ(server.HandleMessage(fwuActivateId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Abort_WithoutObserver_Acknowledges)
    {
        ExpectAck(fwuAbortId);

        EXPECT_EQ(server.HandleMessage(fwuAbortId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, QueryProgress_WithoutObserver_ReportsIdle)
    {
        ExpectResponse(fwuProgressResponseId, { static_cast<uint8_t>(FwuState::idle), 0x00, 0x00, 0x00, 0x00 });
        ExpectAck(fwuQueryProgressId);

        EXPECT_EQ(server.HandleMessage(fwuQueryProgressId, hal::Can::Message{}), CanDispatchResult::handled);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, CompletionAfterTheAcknowledgerIsCleared_SendsTheResponseOnly)
    {
        infra::Function<void(FwuError)> onResult;
        EXPECT_CALL(observer, OnVerify(_, _)).WillOnce(SaveArg<1>(&onResult));
        server.HandleMessage(fwuVerifyId, VerifyMessage());

        server.ClearAcknowledger();

        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::ok) });
        onResult(FwuError::ok);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_NotStartedByARefusedBeginUpgrade)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::busy, 0);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::busy), 0x00, 0x00 });
        ExpectAck(fwuBeginUpgradeId, CanAckStatus::categoryError);

        server.HandleMessage(fwuBeginUpgradeId, BeginUpgradeMessage());

        ForwardTime(std::chrono::seconds(60));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_StartsWhenTheApplicationAcceptsBeginUpgrade)
    {
        infra::Function<void(FwuError, uint16_t)> onResult;
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(SaveArg<1>(&onResult));
        server.HandleMessage(fwuBeginUpgradeId, BeginUpgradeMessage());

        ForwardTime(std::chrono::seconds(60));

        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        onResult(FwuError::ok, 4096);

        ForwardTime(std::chrono::seconds(29));

        EXPECT_CALL(observer, OnSessionTimeout());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_NotExtendedByARefusedDataBlock)
    {
        AcceptBeginUpgrade();
        ForwardTime(std::chrono::seconds(20));

        EXPECT_CALL(observer, OnDataBlock(0, _, _)).WillOnce(Invoke([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::writeError);
            }));
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::writeError), 0x00, 0x00 });
        ExpectAck(fwuDataBlockId, CanAckStatus::categoryError);
        server.HandleMessage(fwuDataBlockId, DataBlockMessage(0));

        ForwardTime(std::chrono::seconds(9));

        EXPECT_CALL(observer, OnSessionTimeout());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_NotStartedByADataBlockRefusedOutsideASession)
    {
        EXPECT_CALL(observer, OnDataBlock(0, _, _)).WillOnce(Invoke([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::invalidState);
            }));
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::invalidState), 0x00, 0x00 });
        ExpectAck(fwuDataBlockId, CanAckStatus::categoryError);
        server.HandleMessage(fwuDataBlockId, DataBlockMessage(0));

        ForwardTime(std::chrono::seconds(60));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_RestartedByADataBlockAcceptedAfterARefusedVerify)
    {
        AcceptBeginUpgrade();

        EXPECT_CALL(observer, OnVerify(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::invalidState);
            }));
        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::invalidState) });
        ExpectAck(fwuVerifyId, CanAckStatus::categoryError);
        server.HandleMessage(fwuVerifyId, VerifyMessage());

        EXPECT_CALL(observer, OnDataBlock(0, _, _)).WillOnce(Invoke([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::ok), 0x00, 0x00 });
        ExpectAck(fwuDataBlockId);
        server.HandleMessage(fwuDataBlockId, DataBlockMessage(0));

        ForwardTime(std::chrono::seconds(29));

        EXPECT_CALL(observer, OnSessionTimeout());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, SessionTimeout_DefaultsToThirtySeconds)
    {
        FirmwareUpgradeCategoryServer defaultServer{ transport };
        defaultServer.SetAcknowledger(acknowledger);
        StrictMock<FirmwareUpgradeCategoryServerObserverMock> defaultObserver{ defaultServer };

        EXPECT_CALL(defaultObserver, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        defaultServer.HandleMessage(fwuBeginUpgradeId, BeginUpgradeMessage());

        ForwardTime(std::chrono::seconds(29));

        EXPECT_CALL(defaultObserver, OnSessionTimeout());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_FiredAfterInactivity)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(6u, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnSessionTimeout());
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, data);

        ForwardTime(std::chrono::seconds(30));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_ResetByDataBlock)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnDataBlock(0, _, _)).WillOnce(Invoke([](uint16_t, infra::ConstByteRange, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuDataBlockAckId, { static_cast<uint8_t>(FwuError::ok), 0x00, 0x00 });
        ExpectAck(fwuDataBlockId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(20));

        hal::Can::Message block;
        block.resize(8, 0);
        CanFrameCodec::WriteInt16(block, 0, 0);
        server.HandleMessage(fwuDataBlockId, block);

        ForwardTime(std::chrono::seconds(29));

        EXPECT_CALL(observer, OnSessionTimeout());
        ForwardTime(std::chrono::seconds(1));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_StoppedByVerify)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnVerify(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuVerifyResponseId, { static_cast<uint8_t>(FwuError::ok) });
        ExpectAck(fwuVerifyId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message verifyMsg;
        verifyMsg.resize(4, 0);
        CanFrameCodec::WriteInt32(verifyMsg, 0, static_cast<int32_t>(0xDEADBEEFu));
        server.HandleMessage(fwuVerifyId, verifyMsg);

        ForwardTime(std::chrono::seconds(60));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_StoppedByAbort)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnAbort(_)).WillOnce(Invoke([](const infra::Function<void()>& cb)
            {
                cb();
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectAck(fwuAbortId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuAbortId, data);

        ForwardTime(std::chrono::seconds(60));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_StoppedByActivate)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnActivate(_)).WillOnce(Invoke([](const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        ExpectResponse(fwuActivateResponseId, { static_cast<uint8_t>(FwuError::ok) });
        ExpectAck(fwuActivateId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuActivateId, data);

        ForwardTime(std::chrono::seconds(60));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_QueryProgressDoesNotResetTimer)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnQueryProgress(_)).Times(2).WillRepeatedly(Invoke([](const infra::Function<void(FwuState, uint16_t, uint16_t)>& cb)
            {
                cb(FwuState::receiving, 0, 0);
            }));
        EXPECT_CALL(observer, OnSessionTimeout());
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 });
        ExpectAck(fwuBeginUpgradeId);
        EXPECT_CALL(canMock, SendData(
                                 hal::Can::Id::Create29BitId(MakeCanId(CanPriority::response, firmwareUpgradeCategoryId, fwuProgressResponseId, 1u)),
                                 ElementsAreArray(std::initializer_list<uint8_t>{ static_cast<uint8_t>(FwuState::receiving), 0x00, 0x00, 0x00, 0x00 }),
                                 testing::_))
            .Times(2)
            .WillRepeatedly([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                });
        EXPECT_CALL(acknowledger, SendCommandAck(firmwareUpgradeCategoryId, fwuQueryProgressId, CanAckStatus::success)).Times(2);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(20));
        hal::Can::Message query;
        server.HandleMessage(fwuQueryProgressId, query);

        ForwardTime(std::chrono::seconds(9));
        server.HandleMessage(fwuQueryProgressId, query);

        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_CustomDuration)
    {
        FirmwareUpgradeCategoryServer::Config shortConfig{ std::chrono::seconds(10) };
        CanFrameTransport transport2{ canMock, 2 };
        FirmwareUpgradeCategoryServer shortServer{ transport2, shortConfig };
        shortServer.SetAcknowledger(acknowledger);
        StrictMock<FirmwareUpgradeCategoryServerObserverMock> obs{ shortServer };

        EXPECT_CALL(obs, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(obs, OnSessionTimeout());
        ExpectResponse(fwuBeginResponseId, { static_cast<uint8_t>(FwuError::ok), 0x10, 0x00 }, 2);
        ExpectAck(fwuBeginUpgradeId);

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        shortServer.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(10));
    }
}
