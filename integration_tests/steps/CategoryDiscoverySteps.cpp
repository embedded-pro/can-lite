#include "cucumber_cpp/Steps.hpp"
#include "support/ApplicationFixture.hpp"
#include "support/ResultTypes.hpp"
#include "gtest/gtest.h"

using namespace services;
using integration::ApplicationFixture;
using integration::CategoryDiscoveryResult;

GIVEN(R"(a custom category with ID {int} is registered on the server)", (std::int32_t categoryId))
{
    auto& fixture = context.Get<ApplicationFixture>();
    fixture.RegisterSimpleCategory(static_cast<uint8_t>(categoryId));
}

WHEN(R"(the client sends a category discovery request to node {int})", (std::int32_t nodeId))
{
    auto& fixture = context.Get<ApplicationFixture>();
    auto result = context.Emplace<CategoryDiscoveryResult>();

    fixture.client.DiscoverCategories(static_cast<uint16_t>(nodeId), [result](const hal::Can::Message& msg)
        {
            result->categories = msg;
            result->received = true;
        });
}

THEN(R"(the category list response shall contain {int} category)", (std::int32_t count))
{
    auto& result = context.Get<CategoryDiscoveryResult>();
    ASSERT_TRUE(result.received);
    EXPECT_EQ(result.categories.size(), static_cast<std::size_t>(count));
}

THEN(R"(the category list response shall contain {int} categories)", (std::int32_t count))
{
    auto& result = context.Get<CategoryDiscoveryResult>();
    ASSERT_TRUE(result.received);
    EXPECT_EQ(result.categories.size(), static_cast<std::size_t>(count));
}

THEN(R"(the category list response shall contain category {int})", (std::int32_t categoryId))
{
    auto& result = context.Get<CategoryDiscoveryResult>();
    ASSERT_TRUE(result.received);

    bool found = false;
    for (std::size_t i = 0; i < result.categories.size(); ++i)
    {
        if (result.categories[i] == static_cast<uint8_t>(categoryId))
        {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Category ID " << categoryId << " not found in response";
}

WHEN(R"(the server sends a category list response without a pending request)")
{
    auto& fixture = context.Get<ApplicationFixture>();

    uint32_t rawId = MakeCanId(CanPriority::response, canSystemCategoryId,
        canCategoryListResponseMessageTypeId, 0);
    auto id = hal::Can::Id::Create29BitId(rawId);
    hal::Can::Message msg;
    msg.push_back(0x00);

    fixture.clientCan.InjectFrame(id, msg);
}

THEN(R"(the client shall silently ignore the response)")
{
    SUCCEED();
}
