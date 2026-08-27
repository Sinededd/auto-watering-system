#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <vector>
#include <MacAddress.h>
#include "host/registry/DeviceRegistry.h"
#include <host/events/EventBus.h>

enum class PairResult
{
    SUCCESS,
    PAIRING,
    ALREADY_PAIRED,
    NOT_IN_BUFFER,
    SAVE_FAILED
};

class DeviceController
{
private:
    static constexpr const char *TAG = "DeviceController";
    static constexpr uint32_t PAIR_TIMEOUT_MS = 10000;

    DeviceRegistry &deviceRegistry;
    EspNowManager &network;
    EventBus &eventBus;

    std::unordered_map<MacAddress, DeviceHeader> pendingPairs;

    void handleSettingsReport(const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
    {
        MacAddress mac(info->src_addr);
        int8_t rssi = info->rx_ctrl->rssi;
        LOG_I(TAG, "Received settings report from %s", mac.toArduinoString().c_str());

        deviceRegistry.processSettingsReport(mac, rssi, pkt);
        deviceRegistry.setDeviceStatus(mac, DeviceStatus::ACTIVE);

        StartWorkModePacket workPkt;
        if (network.send(mac, workPkt))
        {
            LOG_D(TAG, "Success work packet to %s", mac.toArduinoString().c_str());
        }
        else
        {
            LOG_E(TAG, "Error send work packet to %s", mac.toArduinoString().c_str());
        }

        pendingPairs.erase(mac);
        Device *dev = deviceRegistry.findDevice(mac);
        if (dev != nullptr)
        {
            JsonDocument doc = deviceHeaderToJson(dev->header);
            eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());
        }
    }

public:
    explicit DeviceController(DeviceRegistry &deviceRegistry, EspNowManager &net, EventBus &eventBus)
        : deviceRegistry(deviceRegistry), network(net), eventBus(eventBus) {}

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

            if (network.addPeer(mac))
            {
                LOG_D(TAG, "Success add peer with %s", mac.toString().c_str());
            }
            else
            {
                LOG_E(TAG, "Error add peer with %s", mac.toString().c_str());
            }

            PairAcceptPacket acceptPkt;
            if (network.send(mac, acceptPkt))
            {
                LOG_D(TAG, "Success send accept packet to %s", mac.toString().c_str());
            }
            else
            {
                LOG_E(TAG, "Error send accept packet to %s", mac.toString().c_str());
            }

            return PairResult::SUCCESS;
        }

        return PairResult::NOT_IN_BUFFER;
    }

    bool disconnectDevice(const MacAddress &mac)
    {
        pendingPairs.erase(mac);
        network.removePeer(mac);

        bool removed = deviceRegistry.removeDevice(mac);

        if (removed)
            LOG_I(TAG, "Device %s disconnected and removed", mac.toArduinoString().c_str());

        return removed;
    }

    void update()
    {
        uint32_t now = millis();

        for (auto it = pendingPairs.begin(); it != pendingPairs.end();)
        {
            if (now - it->second.lastSeenMs > PAIR_TIMEOUT_MS)
            {
                if (it->second.status == DeviceStatus::UNPAIRED)
                {
                    LOG_I(TAG, "Device lost: %s", it->second.mac.toArduinoString().c_str());
                }
                else
                {
                    LOG_W(TAG, "Pairing timeout for device: %s", it->second.mac.toArduinoString().c_str());
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
        std::strncpy(device.type, packet.type, sizeof(device.type) - 1);
        device.type[sizeof(device.type) - 1] = '\0';
        device.sigLevel = info->rx_ctrl->rssi;
        device.lastSeenMs = millis();

        pendingPairs[mac] = device;

        LOG_I(TAG, "New device found nearby: %s (%s)",
              device.mac.toArduinoString().c_str(),
              device.type);

        JsonDocument doc = deviceHeaderToJson(device);
        eventBus.publish(EventType::DEVICE_DISCOVERED, doc.as<JsonVariantConst>());
    }

    const std::unordered_map<MacAddress, DeviceHeader> &getPendingPairs() const
    {
        return pendingPairs;
    }

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