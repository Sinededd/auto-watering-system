#pragma once

#include "IDeviceState.h"
#include <MacAddress.h>
#include <Packets.h>

class CalibrationState : public IDeviceState
{
private:
    static constexpr const char *TAG = "CALIBRATION_STATE";
    static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
    static constexpr uint32_t SESSION_TIMEOUT_MS = 300000;
    static constexpr uint32_t FINISH_GRACE_MS = 2000;
    static constexpr uint16_t MIN_CALIBRATION_SPAN_MV = 100;

    MacAddress hostMac;
    uint32_t sessionId;
    uint8_t metricIndex;
    uint32_t enteredAtMs = 0;
    uint32_t lastSampleAtMs = 0;
    uint32_t sequence = 0;
    uint32_t leaveAtMs = 0;
    bool leaving = false;
    bool calibrationSaved = false;

    void sendStartResponse(DeviceContext &ctx, bool success, CalibrationError error);
    void handleRepeatedStart(DeviceContext &ctx, const esp_now_recv_info_t *info, const CalibrationStartRequestPacket &packet);
    void handleFinish(DeviceContext &ctx, const esp_now_recv_info_t *info, const CalibrationFinishRequestPacket &packet);
    void handleCancel(DeviceContext &ctx, const esp_now_recv_info_t *info, const CalibrationCancelRequestPacket &packet);

public:
    CalibrationState(const MacAddress &hostMac, uint32_t sessionId, uint8_t metricIndex);

    void onEnter(DeviceContext &ctx) override;
    void onExit(DeviceContext &ctx) override;
    void update(DeviceContext &ctx) override;
};
