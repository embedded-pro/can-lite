#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/test/CanCategoryStubs.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace services;

    class Owner
    {
    public:
        void HandleFrame(const hal::Can::Message& data)
        {
            frameCount++;
            lastFrameSize = data.size();
        }

        bool HandlePdu(infra::ConstByteRange pdu)
        {
            pduCount++;
            lastPduSize = pdu.size();
            return pduAccepted;
        }

        int frameCount = 0;
        int pduCount = 0;
        std::size_t lastFrameSize = 0;
        std::size_t lastPduSize = 0;
        bool pduAccepted = true;
    };

    hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
    {
        hal::Can::Message message;
        for (auto byte : bytes)
            message.push_back(byte);
        return message;
    }

    class HandlerCategory
        : public CanCategoryServerStub
    {
    public:
        HandlerCategory()
        {
            AddMessageTypes(first, second);
        }

        uint8_t Id() const override
        {
            return 0x03;
        }

        void HandleFirst(const hal::Can::Message&)
        {
            firstCount++;
        }

        void HandleSecond(const hal::Can::Message&)
        {
            secondCount++;
        }

        bool HandleSecondPdu(infra::ConstByteRange)
        {
            secondPduCount++;
            return true;
        }

        int firstCount = 0;
        int secondCount = 0;
        int secondPduCount = 0;

    private:
        CanMessageHandler<HandlerCategory> first{ 0x01, *this, &HandlerCategory::HandleFirst };
        CanMessageHandler<HandlerCategory> second{ 0x02, *this, &HandlerCategory::HandleSecond, &HandlerCategory::HandleSecondPdu };
    };
}

TEST(CanMessageHandlerTest, ReportsConfiguredId)
{
    Owner owner;
    CanMessageHandler<Owner> handler{ 0x42, owner, &Owner::HandleFrame };

    EXPECT_EQ(handler.Id(), 0x42);
}

TEST(CanMessageHandlerTest, HandleForwardsFrameToBoundMemberFunction)
{
    Owner owner;
    CanMessageHandler<Owner> handler{ 0x01, owner, &Owner::HandleFrame };

    handler.Handle(MakeMessage({ 0xAA, 0xBB }));

    EXPECT_EQ(owner.frameCount, 1);
    EXPECT_EQ(owner.lastFrameSize, 2u);
    EXPECT_EQ(owner.pduCount, 0);
}

TEST(CanMessageHandlerTest, HandlePduForwardsToBoundMemberFunction)
{
    Owner owner;
    CanMessageHandler<Owner> handler{ 0x01, owner, &Owner::HandleFrame, &Owner::HandlePdu };

    auto pdu = MakeMessage({ 0x01, 0x02, 0x03 });

    EXPECT_TRUE(handler.HandlePdu(infra::MakeRange(pdu)));
    EXPECT_EQ(owner.pduCount, 1);
    EXPECT_EQ(owner.lastPduSize, 3u);
    EXPECT_EQ(owner.frameCount, 0);
}

TEST(CanMessageHandlerTest, HandlePduPropagatesRejection)
{
    Owner owner;
    owner.pduAccepted = false;
    CanMessageHandler<Owner> handler{ 0x01, owner, &Owner::HandleFrame, &Owner::HandlePdu };

    auto pdu = MakeMessage({ 0x01 });

    EXPECT_FALSE(handler.HandlePdu(infra::MakeRange(pdu)));
    EXPECT_EQ(owner.pduCount, 1);
}

TEST(CanMessageHandlerTest, FrameAndPduHandlersAreIndependent)
{
    Owner owner;
    CanMessageHandler<Owner> handler{ 0x01, owner, &Owner::HandleFrame, &Owner::HandlePdu };

    auto pdu = MakeMessage({ 0x01 });
    handler.Handle(MakeMessage({ 0x01, 0x02 }));
    handler.HandlePdu(infra::MakeRange(pdu));

    EXPECT_EQ(owner.frameCount, 1);
    EXPECT_EQ(owner.pduCount, 1);
}

TEST(CanMessageHandlerTest, HandlerWithoutPduSupportAbortsOnPdu)
{
    Owner owner;
    CanMessageHandler<Owner> handler{ 0x01, owner, &Owner::HandleFrame };

    auto pdu = MakeMessage({ 0x01 });

    EXPECT_DEATH(handler.HandlePdu(infra::MakeRange(pdu)), "");
}

TEST(CanMessageHandlerTest, AddMessageTypesRegistersEveryHandler)
{
    HandlerCategory category;

    EXPECT_TRUE(category.HandleMessage(0x01, hal::Can::Message{}));
    EXPECT_TRUE(category.HandleMessage(0x02, hal::Can::Message{}));

    EXPECT_EQ(category.firstCount, 1);
    EXPECT_EQ(category.secondCount, 1);
}

TEST(CanMessageHandlerTest, UnregisteredMessageTypeIsNotDispatched)
{
    HandlerCategory category;

    EXPECT_FALSE(category.HandleMessage(0x7F, hal::Can::Message{}));
    EXPECT_EQ(category.firstCount, 0);
    EXPECT_EQ(category.secondCount, 0);
}

TEST(CanMessageHandlerTest, PduDispatchReachesTheBoundHandler)
{
    HandlerCategory category;
    auto pdu = MakeMessage({ 0x01, 0x02 });

    EXPECT_TRUE(category.HandlePduMessage(0x02, infra::MakeRange(pdu)));
    EXPECT_EQ(category.secondPduCount, 1);
}

TEST(CanMessageHandlerTest, PduDispatchForUnregisteredTypeIsNotHandled)
{
    HandlerCategory category;
    auto pdu = MakeMessage({ 0x01 });

    EXPECT_FALSE(category.HandlePduMessage(0x7F, infra::MakeRange(pdu)));
    EXPECT_EQ(category.secondPduCount, 0);
}

// Transport() is the escape hatch for categories needing a priority or addressing
// combination the Send* helpers do not offer.

class TransportExposingServer
    : public CanCategoryServerStub
{
public:
    uint8_t Id() const override
    {
        return 0x04;
    }

    CanFrameTransport& ExposedTransport()
    {
        return Transport();
    }
};

class TransportExposingClient
    : public CanCategoryClientStub
{
public:
    uint8_t Id() const override
    {
        return 0x05;
    }

    CanFrameTransport& ExposedTransport()
    {
        return Transport();
    }
};

TEST(CanCategoryTransportTest, ServerExposesTheTransportItWasConstructedWith)
{
    TransportExposingServer category;

    EXPECT_EQ(category.ExposedTransport().NodeId(), 0);
}

TEST(CanCategoryTransportTest, ClientExposesTheTransportItWasConstructedWith)
{
    TransportExposingClient category;

    EXPECT_EQ(category.ExposedTransport().NodeId(), 0);
}
