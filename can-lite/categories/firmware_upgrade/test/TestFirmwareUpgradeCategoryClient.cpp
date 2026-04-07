#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
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

    class FirmwareUpgradeCategoryClientObserverMock
        : public FirmwareUpgradeCategoryClientObserver
    {
    public:
        using FirmwareUpgradeCategoryClientObserver::FirmwareUpgradeCategoryClientObserver;

        MOCK_METHOD(void, OnBeginResponse, (FwuError status, uint16_t pageSize), (override));
        MOCK_METHOD(void, OnDataBlockAck, (FwuError status, uint16_t blockIndex), (override));
        MOCK_METHOD(void, OnVerifyResponse, (FwuError status), (override));
        MOCK_METHOD(void, OnActivateResponse, (FwuError status), (override));
        MOCK_METHOD(void, OnProgressResponse, (FwuState state, uint16_t blocksReceived, uint16_t totalBlocks), (override));
    };

    class TestFirmwareUpgradeCategoryClient
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<hal::CanMock> canMock;

        struct FixtureInit
        {
            explicit FixtureInit(StrictMock<hal::CanMock>& canMock)
            {
                EXPECT_CALL(canMock, ReceiveData(_));
                EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        cb(true);
                    }));
            }
        } fixtureInit{ canMock };

        CanProtocolClient protocolClient{ canMock };
        CanFrameTransport transport{ canMock, 1 };
        FirmwareUpgradeCategoryClient client{ transport, protocolClient };
    };

    class TestFirmwareUpgradeCategoryClientWithObserver : public TestFirmwareUpgradeCategoryClient
    {
    public:
        StrictMock<FirmwareUpgradeCategoryClientObserverMock> observer{ client };
    };

    // --- Basic properties ---

    TEST_F(TestFirmwareUpgradeCategoryClient, Id)
    {
        EXPECT_EQ(client.Id(), firmwareUpgradeCategoryId);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, RequiresSequenceValidation_ReturnsFalse)
    {
        EXPECT_FALSE(client.RequiresSequenceValidation());
    }

    // --- Send commands ---

    TEST_F(TestFirmwareUpgradeCategoryClient, SendBeginUpgrade_EncodesSize)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        client.SendBeginUpgrade(1, 393216);

        ASSERT_EQ(captured.size(), 4u);
        auto decoded = CanFrameCodec::ReadUInt32(captured, 0);
        EXPECT_EQ(decoded, 393216u);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendDataBlock_EncodesIndexAndData)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        hal::Can::Message blockData;
        blockData.push_back(0xAA);
        blockData.push_back(0xBB);
        blockData.push_back(0xCC);
        client.SendDataBlock(1, 7, blockData);

        ASSERT_GE(captured.size(), 2u);
        auto blockIndex = CanFrameCodec::ReadUInt16(captured, 0);
        EXPECT_EQ(blockIndex, 7);
        EXPECT_EQ(captured[2], 0xAA);
        EXPECT_EQ(captured[3], 0xBB);
        EXPECT_EQ(captured[4], 0xCC);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendVerify_EncodesCrc32)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        client.SendVerify(1, 0xDEADBEEFu);

        ASSERT_EQ(captured.size(), 4u);
        auto decoded = CanFrameCodec::ReadUInt32(captured, 0);
        EXPECT_EQ(decoded, 0xDEADBEEFu);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendActivate_EmptyPayload)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        client.SendActivate(1);

        EXPECT_TRUE(captured.empty());
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendAbort_EmptyPayload)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        client.SendAbort(1);

        EXPECT_TRUE(captured.empty());
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendDataBlock_RejectsOversizePayload)
    {
        hal::Can::Message blockData;
        for (uint8_t i = 0; i < 7; ++i)
            blockData.push_back(i);

        bool result = client.SendDataBlock(1, 0, blockData);

        EXPECT_FALSE(result);
    }

    // --- Response handling ---

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, BeginResponse_ParsesStatusAndPageSize)
    {
        EXPECT_CALL(observer, OnBeginResponse(FwuError::ok, 4096));

        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(FwuError::ok);
        CanFrameCodec::WriteUInt16(data, 1, 4096);
        client.HandleMessage(fwuBeginResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, BeginResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(2, 0);
        client.HandleMessage(fwuBeginResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, DataBlockAck_ParsesStatusAndIndex)
    {
        EXPECT_CALL(observer, OnDataBlockAck(FwuError::writeError, 5));

        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(FwuError::writeError);
        CanFrameCodec::WriteUInt16(data, 1, 5);
        client.HandleMessage(fwuDataBlockAckId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, VerifyResponse_ParsesStatus)
    {
        EXPECT_CALL(observer, OnVerifyResponse(FwuError::crcMismatch));

        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(FwuError::crcMismatch));
        client.HandleMessage(fwuVerifyResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, VerifyResponse_EmptyIgnored)
    {
        hal::Can::Message data;
        client.HandleMessage(fwuVerifyResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, ActivateResponse_ParsesStatus)
    {
        EXPECT_CALL(observer, OnActivateResponse(FwuError::notReady));

        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(FwuError::notReady));
        client.HandleMessage(fwuActivateResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, ProgressResponse_ParsesAllFields)
    {
        EXPECT_CALL(observer, OnProgressResponse(FwuState::receiving, 100, 2048));

        hal::Can::Message data;
        data.resize(5, 0);
        data[0] = static_cast<uint8_t>(FwuState::receiving);
        CanFrameCodec::WriteUInt16(data, 1, 100);
        CanFrameCodec::WriteUInt16(data, 3, 2048);
        client.HandleMessage(fwuProgressResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, ProgressResponse_TooShortIgnored)
    {
        hal::Can::Message data;
        data.resize(4, 0);
        client.HandleMessage(fwuProgressResponseId, data);
    }

    // --- Boundary values (high-bit fields) ---

    TEST_F(TestFirmwareUpgradeCategoryClient, SendBeginUpgrade_EncodesSize_HighBit)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        client.SendBeginUpgrade(1, 0x80000001u);

        ASSERT_EQ(captured.size(), 4u);
        EXPECT_EQ(CanFrameCodec::ReadUInt32(captured, 0), 0x80000001u);
    }

    TEST_F(TestFirmwareUpgradeCategoryClient, SendDataBlock_EncodesHighBlockIndex)
    {
        hal::Can::Message captured;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(Invoke([&captured](hal::Can::Id, const hal::Can::Message& data, const infra::Function<void(bool)>& cb)
            {
                captured = data;
                cb(true);
            }));

        hal::Can::Message emptyBlock;
        client.SendDataBlock(1, 0x8000u, emptyBlock);

        ASSERT_GE(captured.size(), 2u);
        EXPECT_EQ(CanFrameCodec::ReadUInt16(captured, 0), 0x8000u);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, BeginResponse_ParsesHighPageSize)
    {
        EXPECT_CALL(observer, OnBeginResponse(FwuError::ok, 0xC000u));

        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(FwuError::ok);
        CanFrameCodec::WriteUInt16(data, 1, 0xC000u);
        client.HandleMessage(fwuBeginResponseId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, DataBlockAck_ParsesHighBlockIndex)
    {
        EXPECT_CALL(observer, OnDataBlockAck(FwuError::ok, 0xFFFFu));

        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = static_cast<uint8_t>(FwuError::ok);
        CanFrameCodec::WriteUInt16(data, 1, 0xFFFFu);
        client.HandleMessage(fwuDataBlockAckId, data);
    }

    TEST_F(TestFirmwareUpgradeCategoryClientWithObserver, ProgressResponse_ParsesHighBlockCounts)
    {
        EXPECT_CALL(observer, OnProgressResponse(FwuState::receiving, 0x8001u, 0xFFFFu));

        hal::Can::Message data;
        data.resize(5, 0);
        data[0] = static_cast<uint8_t>(FwuState::receiving);
        CanFrameCodec::WriteUInt16(data, 1, 0x8001u);
        CanFrameCodec::WriteUInt16(data, 3, 0xFFFFu);
        client.HandleMessage(fwuProgressResponseId, data);
    }
}
