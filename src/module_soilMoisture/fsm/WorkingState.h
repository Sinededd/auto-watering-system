#pragma once
#include "IDeviceState.h"

class WorkingState : public IDeviceState {
public:
    void onEnter(DeviceContext& ctx) override;

    void onButtonPressed(DeviceContext& ctx) override;

private:
    void sendTelemetryAndSleep(DeviceContext& ctx, const uint8_t* hostMac);
};