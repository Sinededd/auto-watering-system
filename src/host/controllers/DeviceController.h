#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <vector>
#include <unordered_map>
#include <MacAddress.h>
#include <Packets.h>
#include "host/registry/DeviceRegistry.h"
#include "host/controllers/IrrigationThresholds.h"
#include <host/events/EventBus.h>

enum class PairResult
{
    SUCCESS,
    PAIRING,
    ALREADY_PAIRED,
    NOT_IN_BUFFER,
    SAVE_FAILED
};

struct PendingSettingsTask
{
    MacAddress targetMac;
    DeviceSettings newSettings;
    uint8_t attemptsLeft = 3;
    uint32_t lastAttemptMs = 0;
    static constexpr uint32_t RETRY_INTERVAL_MS = 3000; // Интервал повторов — 3 сек
};

class DeviceController
{
private:
    static constexpr const char *TAG = "DEVICE_CONTROLLER";
    static constexpr uint32_t PAIR_TIMEOUT_MS = 10000;

    DeviceRegistry &deviceRegistry;
    EspNowManager &network;
    EventBus &eventBus;
    IrrigationThresholds &thresholds;

    std::unordered_map<MacAddress, DeviceHeader> pendingPairs;
    std::unordered_map<MacAddress, PendingSettingsTask> pendingSettingsTasks;

    void handleSettingsReport(const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
    {
        MacAddress mac(info->src_addr);
        int8_t rssi = info->rx_ctrl->rssi;
        LOG_I(TAG, "Received settings report from %s", mac.toArduinoString().c_str());

        char devType[12] = "";
        auto it = pendingPairs.find(mac);
        if (it != pendingPairs.end())
        {
            strlcpy(devType, it->second.type, sizeof(devType));
            pendingPairs.erase(it);
        }
        else
        {
            LOG_W(TAG, "Device %s not found in pendingPairs", mac.toString().c_str());
        }

        // 1. Сохраняем устройство в реестре
        deviceRegistry.processSettingsReport(mac, rssi, pkt, devType);
        deviceRegistry.setDeviceStatus(mac, DeviceStatus::ACTIVE);

        // 2. Авто-создание порогов для метрик
        std::string primaryKey = strlen(devType) > 0 ? devType : "";
        uint8_t count = (pkt.interface.metricCount > 3) ? 3 : pkt.interface.metricCount;
        
        for (uint8_t i = 0; i < count; ++i)
        {
            const auto &metric = pkt.interface.metrics[i];
            std::string key = primaryKey.empty() ? metric.label : primaryKey;
            thresholds.autoCreateFromMetric(key, metric);
        }

        // 3. Отправляем подтверждение перевода в рабочий режим
        Device *dev = deviceRegistry.findDevice(mac);
        StartWorkModePacket workPkt;
        if (network.send(mac, workPkt))
        {
            LOG_D(TAG, "Success work packet to %s", mac.toArduinoString().c_str());
        }
        else
        {
            LOG_E(TAG, "Error send work packet to %s", mac.toArduinoString().c_str());
        }

        // 4. Оповещаем систему
        if (dev != nullptr)
        {
            JsonDocument doc = deviceHeaderToJson(dev->header);
            eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());
        }
    }

    void handleSetSettingsResponse(const esp_now_recv_info_t *info, const SetSettingsResponsePacket &pkt)
    {
        MacAddress mac(info->src_addr);
        auto it = pendingSettingsTasks.find(mac);
        if (it == pendingSettingsTasks.end()) return;

        if (pkt.success)
        {
            LOG_I(TAG, "Device %s successfully applied new settings!", mac.toArduinoString().c_str());

            // Обновляем настройки в локальном реестре хоста и сохраняем в NVS
            Device *dev = deviceRegistry.findDevice(mac);
            if (dev)
            {
                dev->settings = it->second.newSettings;
                deviceRegistry.saveDeviceToNvs(*dev);
            }

            JsonDocument doc;
            doc["mac"] = mac.toArduinoString();
            doc["status"] = "success";
            eventBus.publish(EventType::SETTINGS_UPDATE_SUCCESS, doc.as<JsonVariantConst>());
        }
        else
        {
            LOG_E(TAG, "Device %s failed to save settings internally", mac.toArduinoString().c_str());

            JsonDocument doc;
            doc["mac"] = mac.toArduinoString();
            doc["error"] = "Internal device save failure";
            eventBus.publish(EventType::SETTINGS_UPDATE_FAILED, doc.as<JsonVariantConst>());
        }

        pendingSettingsTasks.erase(it);
    }

public:
    explicit DeviceController(
        DeviceRegistry &deviceRegistry, 
        EspNowManager &net, 
        EventBus &eventBus,
        IrrigationThresholds &thresholds
    ) : deviceRegistry(deviceRegistry), network(net), eventBus(eventBus), thresholds(thresholds) {}

    void begin()
    {
        network.onPacket<PairRequestPacket>(
            PacketType::PAIR_REQUEST,
            [this](const esp_now_recv_info_t *info, const PairRequestPacket &pkt)
            {
                this->onPairRequest(info, pkt);
            });

        network.onPacket<SettingsReportPacket>(
            PacketType::SETTINGS_REPORT,
            [this](const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
            {
                this->handleSettingsReport(info, pkt);
            });

        network.onPacket<SetSettingsResponsePacket>(
            PacketType::SET_SETTINGS_RESPONSE,
            [this](const esp_now_recv_info_t *info, const SetSettingsResponsePacket &pkt)
            {
                this->handleSetSettingsResponse(info, pkt);
            });
    }

    PairResult pairDevice(const MacAddress &mac)
    {
        auto it = pendingPairs.find(mac);

        if (it != pendingPairs.end())
        {
            if (it->second.status == DeviceStatus::CONNECTING)
            {
                return PairResult::PAIRING;
            }

            it->second.status = DeviceStatus::CONNECTING;
            it->second.lastSeenMs = millis();

            network.addPeer(mac);

            PairAcceptPacket acceptPkt;
            network.send(mac, acceptPkt);

            return PairResult::SUCCESS;
        }

        return PairResult::NOT_IN_BUFFER;
    }

   bool requestSettingsUpdate(const MacAddress &mac, const DeviceSettings &newSettings)
    {
        Device *dev = deviceRegistry.findDevice(mac);
        if (!dev)
        {
            LOG_W(TAG, "Cannot update settings: device %s not found in registry", mac.toArduinoString().c_str());
            return false;
        }

        if (pendingSettingsTasks.find(mac) != pendingSettingsTasks.end())
        {
            LOG_W(TAG, "Settings update task already pending for %s", mac.toArduinoString().c_str());
            return false;
        }

        network.addPeer(mac);

        PendingSettingsTask task;
        task.targetMac = mac;
        task.newSettings = newSettings;
        task.attemptsLeft = 3;
        task.lastAttemptMs = 0;

        pendingSettingsTasks[mac] = task;

        LOG_I(TAG, "Created settings update task for %s. Awaiting device wakeup...", mac.toArduinoString().c_str());
        return true;
    }

    bool disconnectDevice(const MacAddress &mac)
    {
        pendingPairs.erase(mac);
        pendingSettingsTasks.erase(mac);
        network.removePeer(mac);
        return deviceRegistry.removeDevice(mac);
    }

    void update()
    {
        uint32_t now = millis();

        // 1. Очистка устаревших запросов спаривания
        for (auto it = pendingPairs.begin(); it != pendingPairs.end();)
        {
            if (now - it->second.lastSeenMs > PAIR_TIMEOUT_MS)
            {
                if (it->second.status != DeviceStatus::UNPAIRED)
                {
                    JsonDocument doc = deviceHeaderToJson(it->second);
                    doc["error"] = "Таймаут получения настроек";
                    eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());
                }
                it = pendingPairs.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // 2. Обработка отправки запросов смены настроек (3 попытки с интервалом)
        for (auto it = pendingSettingsTasks.begin(); it != pendingSettingsTasks.end();)
        {
            auto &task = it->second;

            if (now - task.lastAttemptMs >= PendingSettingsTask::RETRY_INTERVAL_MS)
            {
                if (task.attemptsLeft > 0)
                {
                    task.attemptsLeft--;
                    task.lastAttemptMs = now;

                    SetSettingsRequestPacket reqPkt;
                    reqPkt.settings = task.newSettings;

                    LOG_I(TAG, "Sending SET_SETTINGS_REQUEST to %s (Attempts left: %u)", 
                          task.targetMac.toArduinoString().c_str(), task.attemptsLeft);

                    network.send(task.targetMac, reqPkt);
                    ++it;
                }
                else
                {
                    // Все 3 попытки исчерпаны
                    LOG_E(TAG, "Failed to update settings for %s: Sensor timeout", task.targetMac.toArduinoString().c_str());

                    JsonDocument doc;
                    doc["mac"] = task.targetMac.toArduinoString();
                    doc["error"] = "Timeout: device did not respond";
                    eventBus.publish(EventType::SETTINGS_UPDATE_FAILED, doc.as<JsonVariantConst>());

                    it = pendingSettingsTasks.erase(it);
                }
            }
            else
            {
                ++it;
            }
        }
    }

    void onPairRequest(const esp_now_recv_info_t *info, const PairRequestPacket &packet)
    {
        MacAddress mac(info->src_addr);
        auto it = pendingPairs.find(mac);

        if (it != pendingPairs.end())
        {
            if (it->second.status == DeviceStatus::UNPAIRED)
            {
                it->second.lastSeenMs = millis();
                it->second.sigLevel = info->rx_ctrl->rssi;
            }
            return;
        }

        DeviceHeader device;
        device.mac = mac;
        device.status = DeviceStatus::UNPAIRED;
        strlcpy(device.type, packet.type, sizeof(device.type));
        device.sigLevel = info->rx_ctrl->rssi;
        device.lastSeenMs = millis();

        pendingPairs[mac] = device;

        JsonDocument doc = deviceHeaderToJson(device);
        eventBus.publish(EventType::DEVICE_DISCOVERED, doc.as<JsonVariantConst>());
    }

    const std::unordered_map<MacAddress, DeviceHeader> &getPendingPairs() const { return pendingPairs; }

    static String resultToString(PairResult res)
    {
        switch (res)
        {
        case PairResult::SUCCESS:
            return "Запрос на подключение отправлен";
        case PairResult::PAIRING:
            return "Идет подключение";
        case PairResult::ALREADY_PAIRED:
            return "Ошибка: Устройство уже привязано";
        case PairResult::NOT_IN_BUFFER:
            return "Ошибка: Устройство не найдено";
        case PairResult::SAVE_FAILED:
            return "Ошибка: Не удалось сохранить в NVS";
        default:
            return "Неизвестная ошибка";
        }
    }
};