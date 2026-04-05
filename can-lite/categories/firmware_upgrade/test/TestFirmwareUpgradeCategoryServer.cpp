#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
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
    using testing::Invoke;
    using testing::NiceMock;
    using testing::StrictMock;

    class FirmwareUpgradeCategoryServerObserverMock
        : public FirmwareUpgradeCategoryServerObserver
    {
    public:
        using FirmwareUpgradeCategoryServerObserver::FirmwareUpgradeCategoryServerObserver;

        MOCK_METHOD(void, OnBeginUpgrade, (uint32_t firmwareSize), (override));
        MOCK_METHOD(void, OnDataBlock, (uint16_t blockIndex, const hal::Can::Message& data), (override));
        MOCK_METHOD(void, OnVerify, (uint32_t expectedCrc32), (override));
        MOCK_METHOD(void, OnActivate, (), (override));
        MOCK_METHOD(void, OnAbort, (), (override));
        MOCK_METHOD(void, OnQueryProgress, (), (override));
        MOCK_METHOD(void, OnSessionTimeout, (), (override));
    };

    class TestFirmwareUpgradeCategoryServer
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestFirmwareUpgradeCategoryServer()
        {
            ON_CALL(canMock, SendData(_, _, _))
                .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
        }

        NiceMock<hal::CanMock> canMock;
        CanFrameTransport transport{ canMock, 1 };
        FirmwareUpgradeCategoryServer::Config config{ std::chrono::seconds(30) };
        FirmwareUpgradeCategoryServer server{ transport, config };
    };

    class TestFirmwareUpgradeCategoryServerWithObserver : public TestFirmwareUpgradeCategoryServer
    {
    public:
        StrictMock<FirmwareUpgradeCategoryServerObserverMock> observer{ server };
    };

    // --- Basic properties ---

    TEST_F(TestFirmwareUpgradeCategoryServer, Id)
    {
        EXPECT_EQ(server.Id(), firmwareUpgradeCategoryId);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, RequiresSequenceValidation_ReturnsFalse)
    {
        EXPECT_FALSE(server.RequiresSequenceValidation());
    }

    // --- Command handling ---

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, BeginUpgrade_ParsesFirmwareSizeAndNotifiesObserver)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(12288));
        EXPECT_CALL(observer, OnSessionTimeout()).Times(0);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 12288);
        server.HandleMessage(fwuBeginUpgradeId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, BeginUpgrade_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        server.HandleMessage(fwuBeginUpgradeId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, DataBlock_ParsesBlockIndexAndNotifiesObserver)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnDataBlock(42, _)).WillOnce([](uint16_t blockIndex, const hal::Can::Message& data)
            {
                EXPECT_EQ(blockIndex, 42);
                EXPECT_GE(data.size(), 2u);
            });

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

    TEST_F(TestFirmwareUpgradeCategoryServer, DataBlock_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(1, 0);
        server.HandleMessage(fwuDataBlockId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Verify_ParsesCrc32)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnVerify(0xABCD1234u));

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, static_cast<int32_t>(0xABCD1234u));
        server.HandleMessage(fwuVerifyId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, Verify_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(3, 0);
        server.HandleMessage(fwuVerifyId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Activate_EmptyPayloadNotifiesObserver)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnActivate());

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuActivateId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, Abort_EmptyPayloadNotifiesObserver)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnAbort());

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, begin);

        hal::Can::Message data;
        server.HandleMessage(fwuAbortId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, QueryProgress_NotifiesObserver)
    {
        hal::Can::Message data;
        EXPECT_CALL(observer, OnQueryProgress());
        server.HandleMessage(fwuQueryProgressId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, QueryProgress_NoObserverDoesNotCrash)
    {
        hal::Can::Message data;
        server.HandleMessage(fwuQueryProgressId, data);
    }

    // --- Session timeout ---

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_FiredAfterInactivity)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(6));
        EXPECT_CALL(observer, OnSessionTimeout());

        hal::Can::Message data;
        data.resize(4, 0);
        CanFrameCodec::WriteInt32(data, 0, 6);
        server.HandleMessage(fwuBeginUpgradeId, data);

        ForwardTime(std::chrono::seconds(30));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_ResetByDataBlock)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnDataBlock(0, _));
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

        // 20 s after reset — not yet timed out; wait another 10 s to trigger
        ForwardTime(std::chrono::seconds(10));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_StoppedByVerify)
    {
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnVerify(_));

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
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnAbort());

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
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnActivate());

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
        EXPECT_CALL(observer, OnBeginUpgrade(_));
        EXPECT_CALL(observer, OnQueryProgress()).Times(2);
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

        // Total 29 s elapsed — timer fires at 30 s from begin, not reset by queries
        ForwardTime(std::chrono::seconds(2));
    }

    TEST_F(TestFirmwareUpgradeCategoryServerWithObserver, SessionTimeout_CustomDuration)
    {
        FirmwareUpgradeCategoryServer::Config shortConfig{ std::chrono::seconds(10) };
        CanFrameTransport transport2{ canMock, 2 };
        FirmwareUpgradeCategoryServer shortServer{ transport2, shortConfig };
        StrictMock<FirmwareUpgradeCategoryServerObserverMock> obs{ shortServer };

        EXPECT_CALL(obs, OnBeginUpgrade(_));
        EXPECT_CALL(obs, OnSessionTimeout());

        hal::Can::Message begin;
        begin.resize(4, 0);
        CanFrameCodec::WriteInt32(begin, 0, 6);
        shortServer.HandleMessage(fwuBeginUpgradeId, begin);

        ForwardTime(std::chrono::seconds(10));
    }

    // --- Response sending ---

    TEST_F(TestFirmwareUpgradeCategoryServer, SendBeginResponse_EncodesStatusAndPageSize)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        server.SendBeginResponse(FwuError::ok, 4096);

        ASSERT_EQ(captured.size(), 3u);
        EXPECT_EQ(captured[0], static_cast<uint8_t>(FwuError::ok));
        EXPECT_EQ(CanFrameCodec::ReadInt16(captured, 1), static_cast<int16_t>(4096));
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, SendDataBlockAck_EncodesStatusAndIndex)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        server.SendDataBlockAck(FwuError::writeError, 7);

        ASSERT_EQ(captured.size(), 3u);
        EXPECT_EQ(captured[0], static_cast<uint8_t>(FwuError::writeError));
        EXPECT_EQ(CanFrameCodec::ReadInt16(captured, 1), 7);
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, SendVerifyResponse_EncodesStatus)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        server.SendVerifyResponse(FwuError::crcMismatch);

        ASSERT_EQ(captured.size(), 1u);
        EXPECT_EQ(captured[0], static_cast<uint8_t>(FwuError::crcMismatch));
    }

    TEST_F(TestFirmwareUpgradeCategoryServer, SendProgressResponse_EncodesAllFields)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        server.SendProgressResponse(FwuState::receiving, 100, 2048);

        ASSERT_EQ(captured.size(), 5u);
        EXPECT_EQ(captured[0], static_cast<uint8_t>(FwuState::receiving));
        EXPECT_EQ(CanFrameCodec::ReadInt16(captured, 1), 100);
        EXPECT_EQ(CanFrameCodec::ReadInt16(captured, 3), 2048);
    }
}
