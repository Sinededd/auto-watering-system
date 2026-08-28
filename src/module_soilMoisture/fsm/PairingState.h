#pragma once
#include "IDeviceState.h"
#include <Packets.h>
#include <MacAddress.h>

class PairingState : public IDeviceState
{
private:
    static constexpr const char *TAG = "PAIRING_STATE";
    uint32_t lastBroadcastMs = 0;
    uint32_t sendSettingsStartMs = 0;
    bool sendSettings = false;
    MacAddress hostMac;

    void handlePairRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const PairAcceptPacket &packet);
    void handleConnectRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const StartWorkModePacket &packet);
    void sendSettingsReport(DeviceContext &ctx);

public:
    void onEnter(DeviceContext &ctx) override;
    void onExit(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
};