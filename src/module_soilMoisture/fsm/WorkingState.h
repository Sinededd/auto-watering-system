#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>

class WorkingState : public IDeviceState
{
private:
    void sendTelemetry(DeviceContext &ctx, const MacAddress &hostMac);

public:
    void onEnter(DeviceContext &ctx) override;
    void onButtonPressed(DeviceContext &ctx) override;
};