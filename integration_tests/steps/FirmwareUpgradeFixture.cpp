#include "steps/FirmwareUpgradeFixture.hpp"

namespace integration
{
    FirmwareUpgradeFixture::FirmwareUpgradeFixture(ApplicationFixture& application)
        : application(application)
    {
        application.server.RegisterCategory(server);
        application.client.RegisterCategory(client);
    }

    FirmwareUpgradeFixture::~FirmwareUpgradeFixture()
    {
        application.client.UnregisterCategory(client);
        application.server.UnregisterCategory(server);
    }
}
