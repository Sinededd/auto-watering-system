#include "PairingState.h"
#include "DeviceContext.h"
#include "WorkingState.h"
#include <WiFi.h>
#include <Logger.h>

void PairingState::handlePairRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const PairAcceptPacket &packet)
{
    hostMac = MacAddress(info->src_addr);
    ctx.getNet().addPeer(hostMac);

    LOG_I(TAG, "Pair accept received! Sending settings to Host: %s", hostMac.toArduinoString().c_str());
    ctx.getLed().fastBlinking();

    sendSettingsReport(ctx);

    sendSettings = true;

    uint32_t now = millis();
    lastBroadcastMs = now;
    sendSettingsStartMs = now;
}

void PairingState::handleConnectRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const StartWorkModePacket &packet)
{
    LOG_I(TAG, "Start working signal received! Saving config...");

    NetworkConfig netCfg{};
    netCfg.hostMac = MacAddress(info->src_addr);
    netCfg.channel = WiFi.channel();
    netCfg.isPaired = true;

    ctx.getStorage().save("net_cfg", netCfg, ctx.getRtcNetCache());

    ctx.transitionTo(std::make_unique<WorkingState>());
}

void PairingState::sendSettingsReport(DeviceContext &ctx)
{
    SettingsReportPacket currentSettings;
    currentSettings.settings = AppConfig::settings();
    currentSettings.interface = AppConfig::interface();
    ctx.getNet().send(hostMac, currentSettings);
}

void PairingState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered PAIRING state. Searching for Host...");
    ctx.getLed().slowBlinking();
    sendSettings = false;

    ctx.getNet().onPacket<PairAcceptPacket>(
        PacketType::PAIR_ACCEPT,
        [this, &ctx](const esp_now_recv_info_t *info, const PairAcceptPacket &packet) {
            this->handlePairRequest(ctx, info, packet);
        });

    ctx.getNet().onPacket<StartWorkModePacket>(
        PacketType::START_WORK_MODE,
        [this, &ctx](const esp_now_recv_info_t *info, const StartWorkModePacket &packet) {
            this->handleConnectRequest(ctx, info, packet);
        });

    ctx.getNet().addPeer(BROADCAST_MAC);
}

void PairingState::onExit(DeviceContext &ctx)
{
    ctx.getNet().removePacketHandler(PacketType::PAIR_ACCEPT);
    ctx.getNet().removePacketHandler(PacketType::START_WORK_MODE);

    ctx.getLed().turnOff();
    LOG_I(TAG, "Leave PAIRING state.");
}

void PairingState::update(DeviceContext &ctx)
{
    uint32_t now = millis();

    if (!sendSettings)
    {
        if (now - lastBroadcastMs > 2500)
        {
            lastBroadcastMs = now;

            PairRequestPacket req;
            char type[] = "soil_mstr";
            memcpy(req.type, type, sizeof(type));
            ctx.getNet().send(BROADCAST_MAC, req);
        }
    }
    else
    {
        if (now - sendSettingsStartMs > 10000)
        {
            LOG_W(TAG, "Host response timeout! Reseting to search mode...");

            sendSettings = false;
            ctx.getLed().slowBlinking();
            ctx.getNet().removePeer(hostMac);

            lastBroadcastMs = now;
            return;
        }

        if (now - lastBroadcastMs > 3000)
        {
            lastBroadcastMs = now;
            LOG_D(TAG, "Retrying settings report send...");
            sendSettingsReport(ctx);
        }
    }
}