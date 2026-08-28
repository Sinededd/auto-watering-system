#pragma once
#include "IDeviceState.h"

class StartState : public IDeviceState
{
private:
    static constexpr const char *TAG = "START_STATE";

public:
    void onEnter(DeviceContext &ctx) override;
};
