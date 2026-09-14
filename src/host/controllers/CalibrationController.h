#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EspNowManager.h>
#include <MacAddress.h>
#include <Packets.h>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <esp_system.h>

#include "host/events/EventBus.h"
#include "host/registry/DeviceRegistry.h"

enum class CalibrationSessionStatus : uint8_t
{
    STARTING,
    STREAMING,
    FINISHING,
    CANCELLING,
    COMPLETED,
    CANCELLED,
    FAILED
};

struct CalibrationOperationResult
{
    bool success;
    String message;
};

struct SensorAwakeWindow
{
    uint32_t receivedAtMs = 0;
    uint32_t remainingAwakeMs = 0;
};

struct CalibrationSession
{
    static constexpr size_t HISTORY_SIZE = 5;

    uint32_t id = 0;
    MacAddress sensorMac;
    uint8_t metricIndex = 0;
    CalibrationSessionStatus status = CalibrationSessionStatus::STARTING;
    String error;

    bool hasLatestSample = false;
    uint16_t latestRawMilliVolts = 0;
    uint32_t latestSampleAtMs = 0;

    bool hasDryPoint = false;
    bool hasWetPoint = false;
    uint16_t dryMilliVolts = 0;
    uint16_t wetMilliVolts = 0;

    std::array<uint16_t, HISTORY_SIZE> history{};
    size_t historyCount = 0;
    size_t historyCursor = 0;

    uint8_t attemptsLeft = 0;
    uint32_t lastRequestAtMs = 0;
    uint32_t streamingStartedAtMs = 0;
};

class CalibrationController
{
private:
    static constexpr const char *TAG = "CALIBRATION_CONTROLLER";
    static constexpr uint32_t REQUEST_RETRY_MS = 750;
    static constexpr uint32_t SAMPLE_FRESHNESS_MS = 2500;
    static constexpr uint32_t SESSION_TIMEOUT_MS = 305000;
    static constexpr uint16_t MIN_CALIBRATION_SPAN_MV = 100;
    static constexpr uint8_t REQUEST_ATTEMPTS = 5;

    DeviceRegistry &deviceRegistry;
    EspNowManager &network;
    EventBus &eventBus;

    std::unordered_map<MacAddress, SensorAwakeWindow> awakeWindows;
    std::unordered_map<MacAddress, CalibrationSession> sessions;

    static bool isTerminal(CalibrationSessionStatus status)
    {
        return status == CalibrationSessionStatus::COMPLETED ||
               status == CalibrationSessionStatus::CANCELLED ||
               status == CalibrationSessionStatus::FAILED;
    }

    static const char *statusToString(CalibrationSessionStatus status)
    {
        switch (status)
        {
        case CalibrationSessionStatus::STARTING: return "starting";
        case CalibrationSessionStatus::STREAMING: return "streaming";
        case CalibrationSessionStatus::FINISHING: return "finishing";
        case CalibrationSessionStatus::CANCELLING: return "cancelling";
        case CalibrationSessionStatus::COMPLETED: return "completed";
        case CalibrationSessionStatus::CANCELLED: return "cancelled";
        case CalibrationSessionStatus::FAILED: return "failed";
        default: return "unknown";
        }
    }

    static const char *errorToString(CalibrationError error)
    {
        switch (error)
        {
        case CalibrationError::NONE: return "none";
        case CalibrationError::INVALID_SESSION: return "invalid session";
        case CalibrationError::INVALID_METRIC: return "invalid metric";
        case CalibrationError::INVALID_VALUES: return "invalid calibration values";
        case CalibrationError::SAVE_FAILED: return "sensor failed to save calibration";
        case CalibrationError::BUSY: return "sensor is busy";
        default: return "unknown error";
        }
    }

    bool isSensorAwake(const MacAddress &mac) const
    {
        auto it = awakeWindows.find(mac);
        if (it == awakeWindows.end())
        {
            return false;
        }

        return millis() - it->second.receivedAtMs <= it->second.remainingAwakeMs;
    }

    uint16_t medianOfHistory(const CalibrationSession &session) const
    {
        std::array<uint16_t, CalibrationSession::HISTORY_SIZE> sorted{};
        std::copy_n(session.history.begin(), session.historyCount, sorted.begin());
        std::sort(sorted.begin(), sorted.begin() + session.historyCount);
        return sorted[session.historyCount / 2];
    }

    void sendStartRequest(CalibrationSession &session)
    {
        CalibrationStartRequestPacket packet;
        packet.sessionId = session.id;
        packet.metricIndex = session.metricIndex;
        network.send(session.sensorMac, packet);
        session.lastRequestAtMs = millis();
        if (session.attemptsLeft > 0) session.attemptsLeft--;
    }

    void sendFinishRequest(CalibrationSession &session)
    {
        CalibrationFinishRequestPacket packet;
        packet.sessionId = session.id;
        packet.metricIndex = session.metricIndex;
        packet.dryMilliVolts = session.dryMilliVolts;
        packet.wetMilliVolts = session.wetMilliVolts;
        network.send(session.sensorMac, packet);
        session.lastRequestAtMs = millis();
        if (session.attemptsLeft > 0) session.attemptsLeft--;
    }

    void sendCancelRequest(CalibrationSession &session)
    {
        CalibrationCancelRequestPacket packet;
        packet.sessionId = session.id;
        network.send(session.sensorMac, packet);
        session.lastRequestAtMs = millis();
        if (session.attemptsLeft > 0) session.attemptsLeft--;
    }

    void handleWakeupReport(const esp_now_recv_info_t *info, const WakeupReportPacket &packet)
    {
        MacAddress mac(info->src_addr);
        awakeWindows[mac] = {millis(), packet.remainingAwakeMs};
        deviceRegistry.updateTelemetry(mac, info->rx_ctrl->rssi);
        LOG_D(TAG, "Sensor %s is awake for another %u ms",
              mac.toArduinoString().c_str(), packet.remainingAwakeMs);
    }

    void handleStartResponse(const esp_now_recv_info_t *info, const CalibrationStartResponsePacket &packet)
    {
        MacAddress mac(info->src_addr);
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.id != packet.sessionId ||
            it->second.status != CalibrationSessionStatus::STARTING)
        {
            return;
        }

        if (packet.success)
        {
            it->second.status = CalibrationSessionStatus::STREAMING;
            it->second.error = "";
            it->second.streamingStartedAtMs = millis();
            LOG_I(TAG, "Calibration session %u is streaming", packet.sessionId);
        }
        else
        {
            it->second.status = CalibrationSessionStatus::FAILED;
            it->second.error = errorToString(packet.error);
        }
    }

    void handleSample(const esp_now_recv_info_t *info, const CalibrationSamplePacket &packet)
    {
        MacAddress mac(info->src_addr);
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.id != packet.sessionId ||
            it->second.status != CalibrationSessionStatus::STREAMING ||
            it->second.metricIndex != packet.metricIndex ||
            packet.rawMilliVolts > 4095)
        {
            return;
        }

        auto &session = it->second;
        session.latestRawMilliVolts = packet.rawMilliVolts;
        session.latestSampleAtMs = millis();
        session.hasLatestSample = true;
        session.history[session.historyCursor] = packet.rawMilliVolts;
        session.historyCursor = (session.historyCursor + 1) % CalibrationSession::HISTORY_SIZE;
        if (session.historyCount < CalibrationSession::HISTORY_SIZE) session.historyCount++;

        deviceRegistry.updateTelemetry(mac, info->rx_ctrl->rssi);

        JsonDocument event;
        event["mac"] = mac.toArduinoString();
        event["sessionId"] = session.id;
        event["sequence"] = packet.sequence;
        event["metricIndex"] = packet.metricIndex;
        event["rawMilliVolts"] = packet.rawMilliVolts;
        eventBus.publish(EventType::CALIBRATION_SAMPLE, event.as<JsonVariantConst>());
    }

    void handleFinishResponse(const esp_now_recv_info_t *info, const CalibrationFinishResponsePacket &packet)
    {
        MacAddress mac(info->src_addr);
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.id != packet.sessionId ||
            it->second.status != CalibrationSessionStatus::FINISHING)
        {
            return;
        }

        auto &session = it->second;
        if (!packet.success)
        {
            session.status = CalibrationSessionStatus::FAILED;
            session.error = errorToString(packet.error);
            return;
        }

        Device *device = deviceRegistry.findDevice(mac);
        if (!device || session.metricIndex >= device->interface.metricCount)
        {
            session.status = CalibrationSessionStatus::FAILED;
            session.error = "host device registry is inconsistent";
            return;
        }

        auto &calibration = device->interface.metrics[session.metricIndex].calib;
        calibration.pointsCount = 2;
        calibration.rawValues[0] = session.dryMilliVolts;
        calibration.rawValues[1] = session.wetMilliVolts;

        if (!deviceRegistry.saveDeviceToNvs(*device))
        {
            session.status = CalibrationSessionStatus::FAILED;
            session.error = "host failed to save calibration";
            return;
        }

        session.status = CalibrationSessionStatus::COMPLETED;
        session.error = "";
        LOG_I(TAG, "Calibration session %u completed", packet.sessionId);
    }

    void handleCancelResponse(const esp_now_recv_info_t *info, const CalibrationCancelResponsePacket &packet)
    {
        MacAddress mac(info->src_addr);
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.id != packet.sessionId ||
            it->second.status != CalibrationSessionStatus::CANCELLING)
        {
            return;
        }

        it->second.status = CalibrationSessionStatus::CANCELLED;
        it->second.error = "";
    }

public:
    CalibrationController(
        DeviceRegistry &deviceRegistry,
        EspNowManager &network,
        EventBus &eventBus)
        : deviceRegistry(deviceRegistry), network(network), eventBus(eventBus) {}

    void begin()
    {
        network.onPacket<WakeupReportPacket>(
            PacketType::WAKEUP_REPORT,
            [this](const esp_now_recv_info_t *info, const WakeupReportPacket &packet) {
                handleWakeupReport(info, packet);
            });

        network.onPacket<CalibrationStartResponsePacket>(
            PacketType::CALIBRATION_START_RESPONSE,
            [this](const esp_now_recv_info_t *info, const CalibrationStartResponsePacket &packet) {
                handleStartResponse(info, packet);
            });

        network.onPacket<CalibrationSamplePacket>(
            PacketType::CALIBRATION_SAMPLE,
            [this](const esp_now_recv_info_t *info, const CalibrationSamplePacket &packet) {
                handleSample(info, packet);
            });

        network.onPacket<CalibrationFinishResponsePacket>(
            PacketType::CALIBRATION_FINISH_RESPONSE,
            [this](const esp_now_recv_info_t *info, const CalibrationFinishResponsePacket &packet) {
                handleFinishResponse(info, packet);
            });

        network.onPacket<CalibrationCancelResponsePacket>(
            PacketType::CALIBRATION_CANCEL_RESPONSE,
            [this](const esp_now_recv_info_t *info, const CalibrationCancelResponsePacket &packet) {
                handleCancelResponse(info, packet);
            });
    }

    CalibrationOperationResult start(const MacAddress &mac, uint8_t metricIndex)
    {
        Device *device = deviceRegistry.findDevice(mac);
        if (!device)
        {
            return {false, "Устройство не найдено в реестре хоста"};
        }

        if (metricIndex >= device->interface.metricCount)
        {
            return {false, "Указан несуществующий индекс метрики"};
        }

        auto existing = sessions.find(mac);
        if (existing != sessions.end() && !isTerminal(existing->second.status))
        {
            return {false, "Для этого датчика уже выполняется калибровка"};
        }

        if (!isSensorAwake(mac))
        {
            return {false, "Датчик не сообщил о пробуждении. Нажмите кнопку на датчике и повторите запрос"};
        }

        network.addPeer(mac);

        CalibrationSession session;
        session.id = esp_random();
        if (session.id == 0) session.id = 1;
        session.sensorMac = mac;
        session.metricIndex = metricIndex;
        session.status = CalibrationSessionStatus::STARTING;
        session.attemptsLeft = REQUEST_ATTEMPTS;

        sessions[mac] = session;
        sendStartRequest(sessions[mac]);

        return {true, "Команда запуска калибровки отправлена датчику"};
    }

    CalibrationOperationResult capture(const MacAddress &mac, const String &point)
    {
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.status != CalibrationSessionStatus::STREAMING)
        {
            return {false, "Активная калибровочная сессия не найдена"};
        }

        auto &session = it->second;
        if (!session.hasLatestSample ||
            millis() - session.latestSampleAtMs > SAMPLE_FRESHNESS_MS ||
            session.historyCount == 0)
        {
            return {false, "Нет свежих измерений от датчика"};
        }

        uint16_t captured = medianOfHistory(session);
        if (point == "dry")
        {
            session.dryMilliVolts = captured;
            session.hasDryPoint = true;
            return {true, "Сухая точка зафиксирована: " + String(captured) + " мВ"};
        }

        if (point == "wet")
        {
            session.wetMilliVolts = captured;
            session.hasWetPoint = true;
            return {true, "Влажная точка зафиксирована: " + String(captured) + " мВ"};
        }

        return {false, "Точка должна иметь значение 'dry' или 'wet'"};
    }

    CalibrationOperationResult finish(const MacAddress &mac)
    {
        auto it = sessions.find(mac);
        if (it == sessions.end() || it->second.status != CalibrationSessionStatus::STREAMING)
        {
            return {false, "Активная калибровочная сессия не найдена"};
        }

        auto &session = it->second;
        if (!session.hasDryPoint || !session.hasWetPoint)
        {
            return {false, "Сначала зафиксируйте точки 'dry' и 'wet'"};
        }

        int span = abs(static_cast<int>(session.dryMilliVolts) - static_cast<int>(session.wetMilliVolts));
        if (span < MIN_CALIBRATION_SPAN_MV)
        {
            return {false, "Разница между сухой и влажной точками должна быть не меньше 100 мВ"};
        }

        session.status = CalibrationSessionStatus::FINISHING;
        session.attemptsLeft = REQUEST_ATTEMPTS;
        sendFinishRequest(session);
        return {true, "Калибровка отправлена датчику для сохранения"};
    }

    CalibrationOperationResult cancel(const MacAddress &mac)
    {
        auto it = sessions.find(mac);
        if (it == sessions.end() || isTerminal(it->second.status))
        {
            return {false, "Активная калибровочная сессия не найдена"};
        }

        if (it->second.status == CalibrationSessionStatus::FINISHING)
        {
            return {false, "Нельзя отменить калибровку во время сохранения"};
        }

        it->second.status = CalibrationSessionStatus::CANCELLING;
        it->second.attemptsLeft = REQUEST_ATTEMPTS;
        sendCancelRequest(it->second);
        return {true, "Команда отмены отправлена датчику"};
    }

    CalibrationOperationResult writeStatus(const MacAddress &mac, JsonObject out) const
    {
        auto it = sessions.find(mac);
        if (it == sessions.end())
        {
            return {false, "Калибровочная сессия для этого датчика не найдена"};
        }

        const auto &session = it->second;
        out["sessionId"] = session.id;
        out["state"] = statusToString(session.status);
        out["metricIndex"] = session.metricIndex;
        out["startWindowOpen"] = isSensorAwake(mac);

        if (session.hasLatestSample)
        {
            out["latestRawMilliVolts"] = session.latestRawMilliVolts;
            out["sampleAgeMs"] = millis() - session.latestSampleAtMs;
            out["sampleFresh"] = millis() - session.latestSampleAtMs <= SAMPLE_FRESHNESS_MS;
        }
        else
        {
            out["latestRawMilliVolts"] = nullptr;
            out["sampleAgeMs"] = nullptr;
            out["sampleFresh"] = false;
        }

        if (session.hasDryPoint)
        {
            out["dryMilliVolts"] = session.dryMilliVolts;
        }
        else
        {
            out["dryMilliVolts"] = nullptr;
        }

        if (session.hasWetPoint)
        {
            out["wetMilliVolts"] = session.wetMilliVolts;
        }
        else
        {
            out["wetMilliVolts"] = nullptr;
        }

        if (session.error.length() > 0) out["error"] = session.error;
        return {true, "OK"};
    }

    void update()
    {
        uint32_t now = millis();

        for (auto &[mac, session] : sessions)
        {
            if (session.status == CalibrationSessionStatus::STREAMING)
            {
                if (now - session.streamingStartedAtMs > SESSION_TIMEOUT_MS)
                {
                    session.status = CalibrationSessionStatus::FAILED;
                    session.error = "calibration session timeout";
                }
                continue;
            }

            if (session.status != CalibrationSessionStatus::STARTING &&
                session.status != CalibrationSessionStatus::FINISHING &&
                session.status != CalibrationSessionStatus::CANCELLING)
            {
                continue;
            }

            if (now - session.lastRequestAtMs < REQUEST_RETRY_MS)
            {
                continue;
            }

            if (session.attemptsLeft == 0)
            {
                session.status = CalibrationSessionStatus::FAILED;
                session.error = "sensor response timeout";
                continue;
            }

            if (session.status == CalibrationSessionStatus::STARTING)
            {
                sendStartRequest(session);
            }
            else if (session.status == CalibrationSessionStatus::FINISHING)
            {
                sendFinishRequest(session);
            }
            else
            {
                sendCancelRequest(session);
            }
        }
    }
};
