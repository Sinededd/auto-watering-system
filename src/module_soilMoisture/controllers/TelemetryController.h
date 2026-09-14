#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <module_soilMoisture/hardware/SoilMoistureSensor.h>

class DeviceContext;

class TelemetryController {
private:
    static constexpr const char *TAG = "TELEMETRY_CONTROLLER";
    EspNowManager& net;
    SoilMoistureSensor& sensor;

public:
    TelemetryController(EspNowManager& net, SoilMoistureSensor& sensor);
    
    void sendTelemetry(const MacAddress &targetMac);
    void startContinuousMeasurement();
    void stopMeasurement();
    int readRawMilliVolts();
    void setCalibration(int dryMilliVolts, int wetMilliVolts);
};
