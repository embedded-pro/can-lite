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

        for (auto& cat : sequencedCategories)
            server.UnregisterCategory(*cat);
        for (auto& cat : simpleCategories)
            server.UnregisterCategory(*cat);
    }

    void ApplicationFixture::RegisterFocMotor()
    {
        serverTransport.emplace(serverCan, config.nodeId);
        motorServer.emplace(*serverTransport);
        motorServerObserver.emplace(*motorServer);
        server.RegisterCategory(*motorServer);

        clientTransport.emplace(clientCan, config.nodeId);
        motorClient.emplace(*clientTransport);
        motorClientObserver.emplace(*motorClient);
        client.RegisterCategory(*motorClient);
    }

    void ApplicationFixture::RegisterFocMotorServerOnly()
    {
        serverTransport.emplace(serverCan, config.nodeId);
        motorServer.emplace(*serverTransport);
        motorServerObserver.emplace(*motorServer);
        server.RegisterCategory(*motorServer);
    }

    SequencedTestCategory& ApplicationFixture::RegisterSequencedCategory(uint8_t id)
    {
        sequencedCategories.push_back(std::make_unique<SequencedTestCategory>(id));
        auto& cat = *sequencedCategories.back();
        server.RegisterCategory(cat);
        return cat;
    }

    SimpleTestCategory& ApplicationFixture::RegisterSimpleCategory(uint8_t id)
    {
        simpleCategories.push_back(std::make_unique<SimpleTestCategory>(id));
        auto& cat = *simpleCategories.back();
        server.RegisterCategory(cat);
        return cat;
    }

    SequencedTestCategory* ApplicationFixture::FindSequencedCategory(uint8_t id)
    {
        for (auto& cat : sequencedCategories)
            if (cat->Id() == id)
                return cat.get();
        return nullptr;
    }
}
