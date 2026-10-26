#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "gtest/gtest.h"
#include <array>
#include <string>
#include <utility>

using namespace services;
using integration::ApplicationFixture;

namespace
{
    iso_tp::AbortReason AbortReasonNamed(const std::string& name)
    {
        static constexpr std::array<std::pair<const char*, iso_tp::AbortReason>, 7> reasons{ {
            { "nBsTimeout", iso_tp::AbortReason::nBsTimeout },
            { "nCrTimeout", iso_tp::AbortReason::nCrTimeout },
            { "overflow", iso_tp::AbortReason::overflow },
            { "unexpectedFrame", iso_tp::AbortReason::unexpectedFrame },
            { "waitLimitExceeded", iso_tp::AbortReason::waitLimitExceeded },
            { "sendFailed", iso_tp::AbortReason::sendFailed },
            { "nAsTimeout", iso_tp::AbortReason::nAsTimeout },
        } };

        for (const auto& [reasonName, reason] : reasons)
            if (name == reasonName)
                return reason;

        ADD_FAILURE() << "Unknown abort reason " << name;
        return iso_tp::AbortReason::unexpectedFrame;
    }
}

GIVEN(R"(ISO-TP transports connect the client and the server)")
{
    context.Get<ApplicationFixture>().ConnectIsoTp();
}

GIVEN(R"(the bus loses the server's flow control frames)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.serverCan.LoseFramesOn(hal::Can::Id::Create29BitId(fixture.PduFlowControlId()));
}

GIVEN(R"(the client's driver fails every frame of the transfer)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.clientCan.FailFramesOn(hal::Can::Id::Create29BitId(fixture.PduDataId()));
}

WHEN(R"(the bus delivers every frame again)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.serverCan.DeliverEveryFrame();
    fixture.clientCan.DeliverEveryFrame();
}

WHEN(R"(the client application sends a {int}-byte PDU to the PDU test category)", (std::int32_t size))
{
    auto& fixture = context.Get<ApplicationFixture>();

    std::array<uint8_t, 64> pdu{};
    for (std::size_t i = 0; i != pdu.size(); ++i)
        pdu[i] = static_cast<uint8_t>(i);

    EXPECT_TRUE(fixture.clientIsoTp->SendPdu(fixture.PduDataId(), fixture.PduFlowControlId(),
        infra::ConstByteRange(pdu.data(), pdu.data() + size), [&fixture]()
        {
            ++fixture.clientTransfersCompleted;
        }));
}

THEN(R"(the PDU test category shall have received a {int}-byte PDU)", (std::int32_t size))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_EQ(fixture.pduCategory->received.size(), static_cast<std::size_t>(size));
    for (std::size_t i = 0; i != fixture.pduCategory->received.size(); ++i)
        EXPECT_EQ(fixture.pduCategory->received[i], static_cast<uint8_t>(i));
}

THEN(R"(the PDU test category shall have received {int} PDU(s))", (std::int32_t count))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_EQ(fixture.pduCategory->pduCount, count);
}

THEN(R"(the client application shall have been told {int} transfer(s) completed)", (std::int32_t count))
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_EQ(fixture.clientTransfersCompleted, count);
}

THEN(R"(the client application shall have been told the transfer failed with reason {word})", (std::string reason))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.clientIsoTpFailure.has_value());
    EXPECT_EQ(*fixture.clientIsoTpFailure, AbortReasonNamed(reason));
}

THEN(R"(the server application shall have been told the reception failed with reason {word})", (std::string reason))
{
    auto& fixture = context.Get<ApplicationFixture>();
    ASSERT_TRUE(fixture.serverIsoTpFailure.has_value());
    EXPECT_EQ(*fixture.serverIsoTpFailure, AbortReasonNamed(reason));
}

THEN(R"(neither application shall have been told of a failed transfer)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_FALSE(fixture.clientIsoTpFailure.has_value());
    EXPECT_FALSE(fixture.serverIsoTpFailure.has_value());
}

THEN(R"(the server application shall not have been told of a failed reception)")
{
    auto& fixture = context.Get<ApplicationFixture>();
    EXPECT_FALSE(fixture.serverIsoTpFailure.has_value());
}
