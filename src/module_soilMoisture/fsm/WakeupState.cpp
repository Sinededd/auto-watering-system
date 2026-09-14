#include "WakeupState.h"
#include <Logger.h>
#include "DeviceContext.h"
#include "PairingState.h"
#include "WorkingState.h"
#include "CalibrationState.h"

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

    ctx.getNet().onPacket<CalibrationStartRequestPacket>(
        PacketType::CALIBRATION_START_REQUEST,
        [this, &ctx](const esp_now_recv_info_t *info, const CalibrationStartRequestPacket &packet) {
            handleCalibrationStart(ctx, info, packet);
        }
    );

    enteredAtMs = millis();
    ctx.getTelemetryController().sendTelemetry(hostMAC);
    sendWakeupReport(ctx);
}

void WakeupState::onExit(DeviceContext &ctx)
{
    LOG_I(TAG, "Exiting WAKEUP state.");
    
    // Удаляем обработчик пакета, чтобы избежать обращения к удаленному объекту состояния
    ctx.getNet().removePacketHandler(PacketType::SET_SETTINGS_REQUEST);
    ctx.getNet().removePacketHandler(PacketType::CALIBRATION_START_REQUEST);
    ctx.getLed().turnOff();
}

void WakeupState::handleCalibrationStart(
    DeviceContext &ctx,
    const esp_now_recv_info_t *info,
    const CalibrationStartRequestPacket &packet)
{
    MacAddress sender(info->src_addr);
    if (!(sender == hostMAC))
    {
        return;
    }

    if (packet.metricIndex >= AppConfig::interface().metricCount)
    {
        CalibrationStartResponsePacket response;
        response.sessionId = packet.sessionId;
        response.success = false;
        response.error = CalibrationError::INVALID_METRIC;
        ctx.getNet().send(hostMAC, response);
        return;
    }

    LOG_I(TAG, "Starting calibration session %u", packet.sessionId);
    ctx.transitionTo(std::make_unique<CalibrationState>(
        hostMAC,
        packet.sessionId,
        packet.metricIndex));
}

void WakeupState::sendWakeupReport(DeviceContext &ctx)
{
    uint32_t elapsed = millis() - enteredAtMs;
    WakeupReportPacket report;
    report.remainingAwakeMs = elapsed < AWAKE_WINDOW_MS
        ? AWAKE_WINDOW_MS - elapsed
        : 0;

    ctx.getNet().send(hostMAC, report);
    lastAnnouncementMs = millis();
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
    LOG_I(TAG, "Button pressed! Restarting calibration window.");
    enteredAtMs = millis();
    sendWakeupReport(ctx);
}

void WakeupState::update(DeviceContext &ctx)
{
    uint32_t now = millis();

    if (now - enteredAtMs >= AWAKE_WINDOW_MS)
    {
        LOG_I(TAG, "Calibration start window expired");
        ctx.transitionTo(std::make_unique<WorkingState>());
        return;
    }

    if (now - lastAnnouncementMs >= ANNOUNCE_INTERVAL_MS)
    {
        sendWakeupReport(ctx);
    }
}
