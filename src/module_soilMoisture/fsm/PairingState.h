#pragma once
#include "IDeviceState.h"
#include <Packets.h>

class PairingState : public IDeviceState
{
private:
    uint32_t lastBroadcastMs = 0;

    void handlePairRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const PairRequestPacket &packet);

public:
    void onEnter(DeviceContext &ctx) override;
    void onExit(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
};