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
    {}

    ApplicationFixture::~ApplicationFixture()
    {
        if (fwuClient)
            client.UnregisterCategory(*fwuClient);
        if (fwuServer)
            server.UnregisterCategory(*fwuServer);

        for (auto& cat : sequencedCategories)
            server.UnregisterCategory(cat);
        for (auto& cat : simpleCategories)
            server.UnregisterCategory(cat);
    }

    void ApplicationFixture::RegisterFirmwareUpgrade()
    {
        services::FirmwareUpgradeCategoryServer::Config fwuConfig{};
        fwuServer.emplace(server.Transport(), fwuConfig);
        fwuServerObserver.emplace(*fwuServer);
        server.RegisterCategory(*fwuServer);

        fwuClientTransport.emplace(clientCan, config.nodeId);
        fwuClient.emplace(*fwuClientTransport);
        fwuClientObserver.emplace(*fwuClient);
        client.RegisterCategory(*fwuClient);
    }

    SequencedTestCategory& ApplicationFixture::RegisterSequencedCategory(uint8_t id)
    {
        sequencedCategories.emplace_back(id);
        auto& cat = sequencedCategories.back();
        server.RegisterCategory(cat);
        return cat;
    }

    SimpleTestCategory& ApplicationFixture::RegisterSimpleCategory(uint8_t id)
    {
        simpleCategories.emplace_back(id);
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
