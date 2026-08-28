#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>

class WakeupState : public IDeviceState
{
private:
    static constexpr const char *TAG = "WAKE_UP_STATE";

    uint32_t lastSentTel = 0;
    uint8_t countSendTel = 0;

    MacAddress hostMAC;
public:
    void onEnter(DeviceContext &ctx) override;
    void onButtonPressed(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
};