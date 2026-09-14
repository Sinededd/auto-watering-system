#include "TelemetryController.h"

TelemetryController::TelemetryController(EspNowManager &net, SoilMoistureSensor &sensor)
    : net(net), sensor(sensor)
{
}

void TelemetryController::sendTelemetry(const MacAddress &targetMac)
{
    LOG_I(TAG, "Sending telemetry to host");

    TelemetryPacket packet;
    packet.count = 1;
    packet.values[0] = sensor.readPercentage();
    net.send(targetMac, packet);
    delay(40);
}

void TelemetryController::startContinuousMeasurement()
{
    sensor.startContinuous();
}

void TelemetryController::stopMeasurement()
{
    sensor.stop();
}

int TelemetryController::readRawMilliVolts()
{
    return sensor.readMilliVolts();
}

void TelemetryController::setCalibration(int dryMilliVolts, int wetMilliVolts)
{
    sensor.setCalibration(dryMilliVolts, wetMilliVolts);
}
