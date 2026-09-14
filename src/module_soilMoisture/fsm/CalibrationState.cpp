#include "CalibrationState.h"

#include <cstdlib>
#include <Logger.h>
#include "DeviceContext.h"
#include "WorkingState.h"

CalibrationState::CalibrationState(
    const MacAddress &hostMac,
    uint32_t sessionId,
    uint8_t metricIndex)
    : hostMac(hostMac), sessionId(sessionId), metricIndex(metricIndex)
{
}

void CalibrationState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Calibration session %u started", sessionId);
    enteredAtMs = millis();
    lastSampleAtMs = enteredAtMs - SAMPLE_INTERVAL_MS;

    ctx.getTelemetryController().startContinuousMeasurement();

    ctx.getNet().onPacket<CalibrationStartRequestPacket>(
        PacketType::CALIBRATION_START_REQUEST,
        [this, &ctx](const esp_now_recv_info_t *info, const CalibrationStartRequestPacket &packet) {
            handleRepeatedStart(ctx, info, packet);
        });

    ctx.getNet().onPacket<CalibrationFinishRequestPacket>(
        PacketType::CALIBRATION_FINISH_REQUEST,
        [this, &ctx](const esp_now_recv_info_t *info, const CalibrationFinishRequestPacket &packet) {
            handleFinish(ctx, info, packet);
        });

    ctx.getNet().onPacket<CalibrationCancelRequestPacket>(
        PacketType::CALIBRATION_CANCEL_REQUEST,
        [this, &ctx](const esp_now_recv_info_t *info, const CalibrationCancelRequestPacket &packet) {
            handleCancel(ctx, info, packet);
        });

    sendStartResponse(ctx, true, CalibrationError::NONE);
}

void CalibrationState::onExit(DeviceContext &ctx)
{
    ctx.getNet().removePacketHandler(PacketType::CALIBRATION_START_REQUEST);
    ctx.getNet().removePacketHandler(PacketType::CALIBRATION_FINISH_REQUEST);
    ctx.getNet().removePacketHandler(PacketType::CALIBRATION_CANCEL_REQUEST);
    ctx.getTelemetryController().stopMeasurement();
    LOG_I(TAG, "Calibration session %u stopped", sessionId);
}

void CalibrationState::sendStartResponse(
    DeviceContext &ctx,
    bool success,
    CalibrationError error)
{
    CalibrationStartResponsePacket response;
    response.sessionId = sessionId;
    response.success = success;
    response.error = error;
    ctx.getNet().send(hostMac, response);
}

void CalibrationState::handleRepeatedStart(
    DeviceContext &ctx,
    const esp_now_recv_info_t *info,
    const CalibrationStartRequestPacket &packet)
{
    MacAddress sender(info->src_addr);
    if (!(sender == hostMac))
    {
        return;
    }

    if (packet.sessionId == sessionId && packet.metricIndex == metricIndex)
    {
        sendStartResponse(ctx, true, CalibrationError::NONE);
        return;
    }

    CalibrationStartResponsePacket response;
    response.sessionId = packet.sessionId;
    response.success = false;
    response.error = CalibrationError::BUSY;
    ctx.getNet().send(hostMac, response);
}

void CalibrationState::handleFinish(
    DeviceContext &ctx,
    const esp_now_recv_info_t *info,
    const CalibrationFinishRequestPacket &packet)
{
    MacAddress sender(info->src_addr);
    if (!(sender == hostMac) || packet.sessionId != sessionId)
    {
        return;
    }

    CalibrationFinishResponsePacket response;
    response.sessionId = sessionId;
    response.success = false;
    response.error = CalibrationError::NONE;

    // FINISH может прийти повторно, если подтверждение потерялось в эфире.
    // Отвечаем ещё раз, но не записываем те же значения в NVS повторно.
    if (leaving)
    {
        response.success = calibrationSaved;
        response.error = calibrationSaved ? CalibrationError::NONE : CalibrationError::BUSY;
        ctx.getNet().send(hostMac, response);
        return;
    }

    if (packet.metricIndex != metricIndex || metricIndex >= AppConfig::interface().metricCount)
    {
        response.error = CalibrationError::INVALID_METRIC;
    }
    else if (packet.dryMilliVolts > 4095 ||
             packet.wetMilliVolts > 4095 ||
             std::abs(static_cast<int>(packet.dryMilliVolts) - static_cast<int>(packet.wetMilliVolts)) < MIN_CALIBRATION_SPAN_MV)
    {
        response.error = CalibrationError::INVALID_VALUES;
    }
    else
    {
        DeviceInterface updatedInterface = AppConfig::interface();
        auto &calibration = updatedInterface.metrics[metricIndex].calib;
        calibration.pointsCount = 2;
        calibration.rawValues[0] = packet.dryMilliVolts;
        calibration.rawValues[1] = packet.wetMilliVolts;

        if (AppConfig::updateInterface(updatedInterface))
        {
            ctx.getTelemetryController().setCalibration(
                packet.dryMilliVolts,
                packet.wetMilliVolts);
            response.success = true;
            calibrationSaved = true;
        }
        else
        {
            response.error = CalibrationError::SAVE_FAILED;
        }
    }

    ctx.getNet().send(hostMac, response);

    if (response.success)
    {
        ctx.getTelemetryController().stopMeasurement();
        leaving = true;
        leaveAtMs = millis() + FINISH_GRACE_MS;
    }
}

void CalibrationState::handleCancel(
    DeviceContext &ctx,
    const esp_now_recv_info_t *info,
    const CalibrationCancelRequestPacket &packet)
{
    MacAddress sender(info->src_addr);
    if (!(sender == hostMac) || packet.sessionId != sessionId)
    {
        return;
    }

    if (leaving)
    {
        if (!calibrationSaved)
        {
            CalibrationCancelResponsePacket repeatedResponse;
            repeatedResponse.sessionId = sessionId;
            ctx.getNet().send(hostMac, repeatedResponse);
        }
        return;
    }

    CalibrationCancelResponsePacket response;
    response.sessionId = sessionId;
    ctx.getNet().send(hostMac, response);

    ctx.getTelemetryController().stopMeasurement();
    leaving = true;
    leaveAtMs = millis() + FINISH_GRACE_MS;
}

void CalibrationState::update(DeviceContext &ctx)
{
    uint32_t now = millis();

    if (leaving)
    {
        if (static_cast<int32_t>(now - leaveAtMs) >= 0)
        {
            ctx.transitionTo(std::make_unique<WorkingState>());
        }
        return;
    }

    if (now - enteredAtMs >= SESSION_TIMEOUT_MS)
    {
        LOG_W(TAG, "Calibration session timed out");
        ctx.transitionTo(std::make_unique<WorkingState>());
        return;
    }

    if (now - lastSampleAtMs >= SAMPLE_INTERVAL_MS)
    {
        int rawMilliVolts = ctx.getTelemetryController().readRawMilliVolts();

        if (rawMilliVolts >= 0 && rawMilliVolts <= 4095)
        {
            CalibrationSamplePacket sample;
            sample.sessionId = sessionId;
            sample.sequence = sequence++;
            sample.metricIndex = metricIndex;
            sample.rawMilliVolts = static_cast<uint16_t>(rawMilliVolts);
            ctx.getNet().send(hostMac, sample);
        }

        lastSampleAtMs = millis();
    }
}
