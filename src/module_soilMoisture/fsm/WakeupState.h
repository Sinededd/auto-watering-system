#pragma once
#include "IDeviceState.h"
#include <MacAddress.h>
#include <Packets.h>

class WakeupState : public IDeviceState
{
private:
    static constexpr const char *TAG = "WAKE_UP_STATE";
    static constexpr uint32_t AWAKE_WINDOW_MS = 30000;
    static constexpr uint32_t ANNOUNCE_INTERVAL_MS = 2000;

    uint32_t enteredAtMs = 0;
    uint32_t lastAnnouncementMs = 0;

    MacAddress hostMAC;

    void handleSettingsRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const SetSettingsRequestPacket &packet);
    void handleCalibrationStart(DeviceContext &ctx, const esp_now_recv_info_t *info, const CalibrationStartRequestPacket &packet);
    void sendWakeupReport(DeviceContext &ctx);

public:
    void onEnter(DeviceContext &ctx) override;
    void onExit(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
    void onButtonPressed(DeviceContext &ctx) override;
};
