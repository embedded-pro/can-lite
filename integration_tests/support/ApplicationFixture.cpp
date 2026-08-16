#include "support/ApplicationFixture.hpp"

namespace integration
{
    ApplicationFixture::Init::Init(services::VirtualCan& server, services::VirtualCan& client)
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
        for (auto& category : echoCategories)
            server.UnregisterCategory(category);
    }

    services::EchoCategoryServer& ApplicationFixture::RegisterEchoCategory(uint8_t id, bool requiresSequenceValidation)
    {
        echoCategories.emplace_back(id, requiresSequenceValidation);
        auto& category = echoCategories.back();
        server.RegisterCategory(category);
        return category;
    }

    services::EchoCategoryServer* ApplicationFixture::FindEchoCategory(uint8_t id)
    {
        for (auto& category : echoCategories)
            if (category.Id() == id)
                return &category;

        return nullptr;
    }
}
