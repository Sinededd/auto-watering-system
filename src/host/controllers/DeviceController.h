#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <vector>
#include <MacAddress.h>
#include <host/events/EventBus.h>

enum class PairResult {
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

    EspNowManager &network;
    EventBus &eventBus;

    std::unordered_map<MacAddress, DeviceHeader> pendingPairs;

    void handleSettingsReport(const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
    {
        LOG_D(TAG, "Get settings from raw MAC");
        // ...
    }

public:
    explicit DeviceController(EspNowManager &net, EventBus &eventBus) 
        : network(net), eventBus(eventBus) {}

    void begin()
    {
        network.onPacket<SettingsReportPacket>(
            PacketType::SETTINGS_REPORT,
            [this](const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
            {
                this->handleSettingsReport(info, pkt);
            });
    }

    PairResult pairDevice(const MacAddress& mac)
    {
        auto it = pendingPairs.find(mac);

        if (it != pendingPairs.end())
        {
            if (it->second.status == DeviceStatus::CONNECTING) {
                return PairResult::PAIRING;
            }

            it->second.status = DeviceStatus::CONNECTING;
            it->second.lastSeenMs = millis();

            network.addPeer(mac, 0);
            PairAcceptPacket acceptPkt;
            network.send(mac, acceptPkt);
            
            return PairResult::SUCCESS;
        }

        return PairResult::NOT_IN_BUFFER;
    }

    void update()
    {
        uint32_t now = millis();

        for (auto it = pendingPairs.begin(); it != pendingPairs.end(); )
        {
            if (now - it->second.lastSeenMs > PAIR_TIMEOUT_MS)
            {
                if (it->second.status == DeviceStatus::UNPAIRED) {
                    LOG_I(TAG, "Device lost: %s", it->second.mac.toArduinoString().c_str());
                } else {
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

    const std::unordered_map<MacAddress, DeviceHeader>& getPendingPairs() const 
    { 
        return pendingPairs; 
    }

    static String resultToString(PairResult res)
    {
        switch (res)
        {
        case PairResult::SUCCESS:      return "Запрос на подключение отправлен";
        case PairResult::PAIRING:      return "Идет подключение";
        case PairResult::ALREADY_PAIRED: return "Ошибка: Устройство уже привязано";
        case PairResult::NOT_IN_BUFFER: return "Ошибка: Устройство не найдено";
        case PairResult::SAVE_FAILED:  return "Ошибка: Не удалось сохранить в NVS";
        default:                        return "Неизвестная ошибка";
        }
    }
};