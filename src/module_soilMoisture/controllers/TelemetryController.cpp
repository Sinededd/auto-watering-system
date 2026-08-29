#include "TelemetryController.h"

TelemetryController::TelemetryController(EspNowManager &net) : net(net)
{
}

void TelemetryController::sendTelemetry(const MacAddress &targetMac)
{
    LOG_I(TAG, "Sending telemetry to host");

    TelemetryPacket packet;
    packet.count = 1;
    packet.values[0] = 52;
    net.send(targetMac, packet);
    delay(40);
}
