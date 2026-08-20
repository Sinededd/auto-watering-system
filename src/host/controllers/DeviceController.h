#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <vector>
#include <host/events/EventBus.h>
#include "DeviceRegistry.h"
#include "PairingBuffer.h"

enum class PairResult
{
    SUCCESS,
    ALREADY_PAIRED,
    NOT_IN_BUFFER,
    SAVE_FAILED
};

struct PendingPairing
{
    uint8_t mac[6];
    uint32_t startTimeMs;
};

class DeviceController
{
private:
    static constexpr const char *TAG = "DeviceController";
    static constexpr uint32_t PAIR_TIMEOUT_MS = 20000;

    DeviceRegistry &registry;
    PairingBuffer &pairingBuff;
    EspNowManager &network;
    EventBus &eventBus;

    std::vector<PendingPairing> pendingPairs;

    void handleSettingsReport(const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
    {
        const uint8_t *mac = info->src_addr;

        std::optional<Device> devOpt = registry.getByMac(mac);
        if (!devOpt.has_value())
        {
            LOG_W(TAG, "Received settings from unknown device: %s", macToString(mac).c_str());
            return;
        }

        removePending(mac);

        Device dev = devOpt.value();
        dev.settings.reportIntervalSec = pkt.reportIntervalSec;
        dev.settings.isValid = true;
        dev.header.status = DeviceStatus::ACTIVE;

        if (!registry.save(dev))
        {
            LOG_E(TAG, "Failed to save device settings %s", macToString(mac).c_str());
            return;
        }

        StartWorkModePacket startPkt;
        startPkt.timestamp = millis() / 1000;
        network.send(mac, startPkt);

        LOG_I(TAG, "Device %s successfully connected and transferred to WORK_MODE", macToString(mac).c_str());

        JsonDocument doc = deviceToJson(dev);
        eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());
    }

    void removePending(const uint8_t *mac)
    {
        for (auto it = pendingPairs.begin(); it != pendingPairs.end(); ++it)
        {
            if (memcmp(it->mac, mac, 6) == 0)
            {
                pendingPairs.erase(it);
                break;
            }
        }
    }

public:
    explicit DeviceController(
        DeviceRegistry &registry,
        PairingBuffer &pairingBuff,
        EspNowManager &net,
        EventBus &bus) : registry(registry), pairingBuff(pairingBuff), network(net), eventBus(bus) {}

    void begin()
    {
        network.onPacket<SettingsReportPacket>(
            PacketType::SETTINGS_REPORT,
            [this](const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
            {
                this->handleSettingsReport(info, pkt);
            });
    }

    void update()
    {
        uint32_t now = millis();

        for (auto it = pendingPairs.begin(); it != pendingPairs.end();)
        {
            if (now - it->startTimeMs > PAIR_TIMEOUT_MS)
            {
                LOG_W(TAG, "Pairing timeout for device: %s", macToString(it->mac).c_str());

                Device failedDev{};
                memcpy(failedDev.header.mac, it->mac, 6);
                failedDev.header.status = DeviceStatus::UNPAIRED;

                JsonDocument doc = deviceToJson(failedDev);
                doc["error"] = "Таймаут получения настроек";
                eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());

                registry.remove(it->mac);

                it = pendingPairs.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    PairResult pairDevice(const uint8_t *mac)
    {
        if (registry.getByMac(mac).has_value())
        {
            return PairResult::ALREADY_PAIRED;
        }

        const DiscoveredDevice *foundInBuff = nullptr;
        for (const auto &dev : pairingBuff.getList())
        {
            if (memcmp(dev.device.header.mac, mac, 6) == 0)
            {
                foundInBuff = &dev;
                break;
            }
        }

        if (!foundInBuff)
        {
            return PairResult::NOT_IN_BUFFER;
        }

        Device newDevice{};
        memcpy(newDevice.header.mac, foundInBuff->device.header.mac, 6);
        newDevice.header.type = foundInBuff->device.header.type;
        newDevice.header.status = DeviceStatus::CONNECTING;
        newDevice.settings.isValid = false;

        if (!registry.save(newDevice))
        {
            return PairResult::SAVE_FAILED;
        }

        network.addPeer(newDevice.header.mac, 0);

        PairAcceptPacket acceptPkt;
        network.send(newDevice.header.mac, acceptPkt);

        pairingBuff.remove(mac);

        PendingPairing pending;
        memcpy(pending.mac, mac, 6);
        pending.startTimeMs = millis();
        pendingPairs.push_back(pending);

        JsonDocument doc = deviceToJson(newDevice);
        eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());

        return PairResult::SUCCESS;
    }

    static String resultToString(PairResult res)
    {
        switch (res)
        {
        case PairResult::SUCCESS:
            return "Запрос на подключение отправлен";
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