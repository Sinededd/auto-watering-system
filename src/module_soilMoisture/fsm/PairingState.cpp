#include "PairingState.h"
#include "DeviceContext.h"
#include "WorkingState.h"
#include <WiFi.h>
#include <Logger.h>

void PairingState::handlePairRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const PairRequestPacket &packet)
{
    NetworkConfig netCfg{};
    memcpy(netCfg.hostMac, info->src_addr, 6);
    netCfg.channel = 0;
    netCfg.isPaired = true;

    ctx.getStorage().save("net_cfg", netCfg, ctx.getRtcNetCache());

    LOG_I("STATE", "Paired! Transitioning to WORKING state.");

    ctx.transitionTo(std::make_unique<WorkingState>());
}

void PairingState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered PAIRING state. Blinking LED, searching for Host...");
    ctx.getLed().slowBlinking();

    ctx.getNet().onPacket<PairRequestPacket>(
        PacketType::PAIR_REQUEST,
        std::bind(&PairingState::handlePairRequest, this, std::ref(ctx), std::placeholders::_1, std::placeholders::_2));

    ctx.getNet().addPeer(BROADCAST_MAC);
}

void PairingState::onExit(DeviceContext &ctx)
{
    ctx.getNet().removePacketHandler(PacketType::PAIR_REQUEST);
    ctx.getLed().turnOff();
    LOG_I(TAG, "Leave PAIRING state.");
}

void PairingState::update(DeviceContext &ctx)
{
    if (millis() - lastBroadcastMs > 1000)
    {
        lastBroadcastMs = millis();

        PairRequestPacket req;
        char type[] = "soil";
        memcpy(req.type, type, sizeof(type));
        ctx.getNet().send(BROADCAST_MAC, req);
    }
}