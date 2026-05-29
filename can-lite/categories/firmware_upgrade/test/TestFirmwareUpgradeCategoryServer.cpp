#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace services;
    using testing::_;
    using testing::AnyNumber;
    using testing::Invoke;
    using testing::StrictMock;

    class FirmwareUpgradeCategoryServerObserverMock
        : public FirmwareUpgradeCategoryServerObserver
    {
    public:
        using FirmwareUpgradeCategoryServerObserver::FirmwareUpgradeCategoryServerObserver;

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, const hal::Can::Message& data, const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnVerify, (uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnActivate, (const infra::Function<void(FwuError)>& onResult), (override));
        MOCK_METHOD(void, OnAbort, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, OnQueryProgress, (const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult), (override));
        MOCK_METHOD(void, OnSessionTimeout, (), (override));
    };

    class AcknowledgerSpy
        : public CanCommandAcknowledger
    {
    public:
        void SendCommandAck(uint8_t categoryId, uint8_t commandType, CanAckStatus status) override
        {
            lastCategoryId = categoryId;
            lastCommandType = commandType;
            lastStatus = status;
            ackCount++;
        }

        uint8_t lastCategoryId{ 0 };
        uint8_t lastCommandType{ 0 };
        CanAckStatus lastStatus{ CanAckStatus::success };
        std::size_t ackCount{ 0 };
    };

    class TestFirmwareUpgradeCategoryServer
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestFirmwareUpgradeCategoryServer()
        {
            EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([this](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
                {
                    lastSentData = data;
                    sendCount++;
                    cb(true);
                }));
            server.SetAcknowledger(acknowledger);
        }

        StrictMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FirmwareUpgradeCategoryServer::Config config{ std::chrono::seconds(30) };
        FirmwareUpgradeCategoryServer server{ transport, config };
        AcknowledgerSpy acknowledger;
        hal::Can::Message lastSentData;
        std::size_t sendCount{ 0 };
    };

    class TestFirmwareUpgradeCategoryServerWithObserver : public TestFirmwareUpgradeCategoryServer
    {
    public:
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

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 12288);
        server.HandleMessage(fwuBeginUpgradeId, data);

        ASSERT_EQ(lastSentData.size(), 3u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(FwuError::ok));
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 1), 4096);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, BeginUpgrade_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        server.HandleMessage(fwuBeginUpgradeId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, DataBlock_CallbackSendsAck)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnDataBlock(42, _, _)).WillOnce(Invoke([](uint16_t blockIndex, const hal::Can::Message& data, const infra::Function<void(FwuError)>& cb)
            {
                EXPECT_EQ(blockIndex, 42);
                EXPECT_EQ(data.size(), 6u);
                cb(FwuError::ok);
            }));

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

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, DataBlock_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        server.HandleMessage(fwuDataBlockId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, static_cast<int32_t>(0xABCD1234u));
        server.HandleMessage(fwuVerifyId, data);

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Verify_TooShortRejected)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        server.HandleMessage(fwuVerifyId, data);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::invalidPayload);
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

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuActivateId, data);

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
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

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuAbortId, data);

        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, QueryProgress_CallbackSendsResponse)
    {
        EXPECT_CALL(observer, OnQueryProgress(_)).WillOnce(Invoke([](const infra::Function<void(FwuState, uint16_t, uint16_t)>& cb)
            {
                cb(FwuState::receiving, 100, 2048);
            }));

        hal::Can::Message data;
        server.HandleMessage(fwuQueryProgressId, data);

        ASSERT_EQ(lastSentData.size(), 5u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(FwuState::receiving));
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 1), 100);
        EXPECT_EQ(CanFrameCodec::ReadInt16(lastSentData, 3), 2048);
        EXPECT_EQ(acknowledger.lastStatus, CanAckStatus::success);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, QueryProgress_NoObserverDoesNotCrash)
    {
        hal::Can::Message data;
        server.HandleMessage(fwuQueryProgressId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_FiredAfterInactivity)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(6u, _)).WillOnce(Invoke([](uint32_t, const infra::Function<void(FwuError, uint16_t)>& cb)
            {
                cb(FwuError::ok, 4096);
            }));
        EXPECT_CALL(observer, OnSessionTimeout());

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
        EXPECT_CALL(observer, OnDataBlock(0, _, _)).WillOnce(Invoke([](uint16_t, const hal::Can::Message&, const infra::Function<void(FwuError)>& cb)
            {
                cb(FwuError::ok);
            }));
        EXPECT_CALL(observer, OnSessionTimeout());

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(20));

        hal::Can::Message block;
        block.resize(8, 0);
        CanFrameCodec::WriteInt16(block, 0, 0);
        server.HandleMessage(fwuDataBlockId, block);

        ForwardTime(std::chrono::seconds(20));
        ForwardTime(std::chrono::seconds(10));
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

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        shortServer.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(10));
    }
}
