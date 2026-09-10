#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>
#include <Packets.h>

class WakeupState : public IDeviceState
{
private:
    static constexpr const char *TAG = "WAKE_UP_STATE";

    uint32_t lastSentTel = 0;
    uint8_t countSendTel = 0;

    MacAddress hostMAC;

    void handleSettingsRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const SetSettingsRequestPacket &packet);

public:
    void onEnter(DeviceContext &ctx) override;
    void onExit(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
    void onButtonPressed(DeviceContext &ctx) override;
};