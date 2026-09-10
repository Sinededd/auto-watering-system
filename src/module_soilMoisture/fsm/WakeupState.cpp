#include "WakeupState.h"
#include <Logger.h>
#include "DeviceContext.h"
#include "PairingState.h"
#include "WorkingState.h"

void WakeupState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered WAKEUP state.");
    ctx.getLed().turnOn();

    // Загрузка настроек сети с использованием RTC кэша
    auto netCfg = ctx.getStorage().load<NetworkConfig>("net_cfg", ctx.getRtcNetCache());
    if (netCfg.has_value())
    {
        this->hostMAC = netCfg->hostMac;
        ctx.getNet().addPeer(netCfg->hostMac, netCfg->channel);
        ctx.getTelemetryController().sendTelemetry(netCfg->hostMac);

        lastSentTel = millis();
        countSendTel = 1;
    }
    else
    {
        LOG_E(TAG, "Failed to add host peer. Switch to Pairing state!");
        ctx.transitionTo(std::make_unique<PairingState>());
        return;
    }

    // Подписка на пакет изменения настроек через EspNowManager
    ctx.getNet().onPacket<SetSettingsRequestPacket>(
        PacketType::SET_SETTINGS_REQUEST,
        [this, &ctx](const esp_now_recv_info_t *info, const SetSettingsRequestPacket &packet) {
            handleSettingsRequest(ctx, info, packet);
        }
    );
}

void WakeupState::onExit(DeviceContext &ctx)
{
    LOG_I(TAG, "Exiting WAKEUP state.");
    
    // Удаляем обработчик пакета, чтобы избежать обращения к удаленному объекту состояния
    ctx.getNet().removePacketHandler(PacketType::SET_SETTINGS_REQUEST);
    ctx.getLed().turnOff();
}

void WakeupState::handleSettingsRequest(DeviceContext &ctx, const esp_now_recv_info_t *info, const SetSettingsRequestPacket &packet)
{
    LOG_I(TAG, "Received new settings request!");

    // 1. Сохраняем новые настройки в NVS и RTC RAM
    bool saved = AppConfig::updateSettings(packet.settings);

    // 2. Формируем и отправляем подтверждение через метод send() библиотеки
    SetSettingsResponsePacket response;
    response.success = saved;

    MacAddress hostMac(info->src_addr);
    ctx.getNet().send(hostMac, response);

    // 3. Индикация и переход в рабочий режим
    if (saved)
    {
        ctx.getLed().fastBlinkingBlocking(3);
        LOG_I(TAG, "Settings applied successfully! Returning to work.");
        ctx.transitionTo(std::make_unique<WorkingState>());
    }
}

void WakeupState::onButtonPressed(DeviceContext &ctx)
{
    LOG_I(TAG, "Button pressed! Resetting telemetry sequence.");

    ctx.getTelemetryController().sendTelemetry(hostMAC);
    lastSentTel = millis();
    countSendTel = 1;
}

void WakeupState::update(DeviceContext &ctx)
{
    uint32_t now = millis();

    if (now - lastSentTel > 3000)
    {
        if (countSendTel >= 3)
        {
            ctx.transitionTo(std::make_unique<WorkingState>());
            return;
        }

        ctx.getTelemetryController().sendTelemetry(hostMAC);
        lastSentTel = now;
        countSendTel++;
    }
}