#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>

class WorkingState : public IDeviceState {
public:
    void onEnter(DeviceContext& ctx) override;

    void onButtonPressed(DeviceContext& ctx) override;

private:
    void sendTelemetryAndSleep(DeviceContext& ctx, const MacAddress& hostMac);
};