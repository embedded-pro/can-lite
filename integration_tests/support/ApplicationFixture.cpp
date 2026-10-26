#include "support/ApplicationFixture.hpp"

namespace integration
{
    ApplicationFixture::Init::Init(VirtualCan& server, VirtualCan& client)
    {
        server.ConnectTo(client);
    }

    ApplicationFixture::ApplicationFixture(uint16_t nodeId, uint16_t rateLimit)
        : init(serverCan, clientCan)
        , config{ nodeId, rateLimit, std::chrono::seconds(1) }
        , server(serverCan, config)
        , serverObserver(server)
        , client(clientCan)
        , clientObserver(client)
    {
        EXPECT_CALL(serverObserver, Online()).WillRepeatedly([this]()
            {
                ++processedCount;
                clientOnlineAsSeenByServer = true;
            });
        EXPECT_CALL(serverObserver, Offline()).WillRepeatedly([this]()
            {
                clientOnlineAsSeenByServer = false;
            });
        EXPECT_CALL(clientObserver, OnServerOnline(testing::_)).WillRepeatedly([this](uint16_t)
            {
                ++serversOnlineAsSeenByClient;
            });
        EXPECT_CALL(clientObserver, OnServerOffline(testing::_)).WillRepeatedly([this](uint16_t)
            {
                --serversOnlineAsSeenByClient;
            });
        EXPECT_CALL(clientObserver, OnCommandAckTimeout(testing::_, testing::_, testing::_)).WillRepeatedly([this](uint16_t, uint8_t, uint8_t)
            {
                ++commandAckTimeouts;
            });
    }

    ApplicationFixture::~ApplicationFixture()
    {
        if (demoClient)
            client.UnregisterCategory(*demoClient);
        if (demoServer)
            server.UnregisterCategory(*demoServer);

        if (fwuClient)
            client.UnregisterCategory(*fwuClient);
        if (fwuServer)
            server.UnregisterCategory(*fwuServer);

        for (auto& cat : sequencedCategories)
            server.UnregisterCategory(cat);
        for (auto& cat : simpleCategories)
            server.UnregisterCategory(cat);
    }

    void ApplicationFixture::RegisterDemoCategory()
    {
        RegisterDemoCategoryServerOnly();

        demoClient.emplace(client.Transport(), client);
        demoClientObserver.emplace(*demoClient);
        client.RegisterCategory(*demoClient);
    }

    void ApplicationFixture::RegisterDemoCategoryServerOnly()
    {
        demoServer.emplace(server.Transport());
        demoServerObserver.emplace(*demoServer);
        server.RegisterCategory(*demoServer);
    }

    void ApplicationFixture::RegisterFirmwareUpgrade()
    {
        services::FirmwareUpgradeCategoryServer::Config fwuConfig{};
        fwuServer.emplace(server.Transport(), fwuConfig);
        fwuServerObserver.emplace(*fwuServer);
        EXPECT_CALL(*fwuServerObserver, OnSessionTimeout()).Times(testing::AtMost(1)).WillOnce([this]()
            {
                fwuSessionTimedOut = true;
            });
        server.RegisterCategory(*fwuServer);

        fwuClient.emplace(client.Transport(), client);
        fwuClientObserver.emplace(*fwuClient);
        client.RegisterCategory(*fwuClient);
    }

    SequencedTestCategory& ApplicationFixture::RegisterSequencedCategory(uint8_t id)
    {
        sequencedCategories.emplace_back(server.Transport(), id);
        auto& cat = sequencedCategories.back();
        server.RegisterCategory(cat);
        return cat;
    }

    SimpleTestCategory& ApplicationFixture::RegisterSimpleCategory(uint8_t id)
    {
        simpleCategories.emplace_back(server.Transport(), id);
        auto& cat = simpleCategories.back();
        server.RegisterCategory(cat);
        return cat;
    }

    SequencedTestCategory* ApplicationFixture::FindSequencedCategory(uint8_t id)
    {
        for (auto& cat : sequencedCategories)
            if (cat.Id() == id)
                return &cat;
        return nullptr;
    }
}
