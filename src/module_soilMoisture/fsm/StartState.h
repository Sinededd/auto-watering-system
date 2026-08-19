#pragma once
#include "IDeviceState.h"

class StartState : public IDeviceState
{
public:
    void onEnter(DeviceContext &ctx) override;
};
