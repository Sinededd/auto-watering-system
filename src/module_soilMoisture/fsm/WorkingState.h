#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>

class WorkingState : public IDeviceState
{
private:
    static constexpr const char *TAG = "WORKING_STATE";

public:
    void onEnter(DeviceContext &ctx) override;
    void onButtonPressed(DeviceContext &ctx) override;
};