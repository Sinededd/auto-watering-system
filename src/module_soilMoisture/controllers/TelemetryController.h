#pragma once
#include <Arduino.h>
#include <EspNowManager.h>

class DeviceContext;

class TelemetryController {
private:
    static constexpr const char *TAG = "TELEMETRY_CONTROLLER";
    EspNowManager& net;

public:
    TelemetryController(EspNowManager& net);
    
    void sendTelemetry(const MacAddress &targetMac);
};