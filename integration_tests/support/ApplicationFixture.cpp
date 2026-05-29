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
        if (motorClient)
            client.UnregisterCategory(*motorClient);
        if (motorServer)
            server.UnregisterCategory(*motorServer);

        if (fwuClient)
            client.UnregisterCategory(*fwuClient);
        if (fwuServer)
            server.UnregisterCategory(*fwuServer);

        for (auto& cat : sequencedCategories)
            server.UnregisterCategory(cat);
        for (auto& cat : simpleCategories)
            server.UnregisterCategory(cat);
    }

    void ApplicationFixture::RegisterFocMotor()
    {
        motorServer.emplace(server.Transport());
        motorServerObserver.emplace(*motorServer);
        server.RegisterCategory(*motorServer);

        clientTransport.emplace(clientCan, config.nodeId);
        motorClient.emplace(*clientTransport, client);
        motorClientObserver.emplace(*motorClient);
        client.RegisterCategory(*motorClient);
    }

    void ApplicationFixture::RegisterFocMotorServerOnly()
    {
        motorServer.emplace(server.Transport());
        motorServerObserver.emplace(*motorServer);
        server.RegisterCategory(*motorServer);
    }

    void ApplicationFixture::RegisterFirmwareUpgrade()
    {
        services::FirmwareUpgradeCategoryServer::Config fwuConfig{};
        fwuServer.emplace(server.Transport(), fwuConfig);
        fwuServerObserver.emplace(*fwuServer);
        server.RegisterCategory(*fwuServer);

        fwuClientTransport.emplace(clientCan, config.nodeId);
        fwuClient.emplace(*fwuClientTransport, client);
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
