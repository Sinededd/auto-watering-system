#pragma once
#include <Arduino.h>
#include <EspNowManager.h>
#include <vector>
#include <MacAddress.h>
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
    MacAddress mac;
    uint32_t startTimeMs;
};

class DeviceController
{
private:
    static constexpr const char *TAG = "DeviceController";
    static constexpr uint32_t PAIR_TIMEOUT_MS = 10000;

    DeviceRegistry &registry;
    PairingBuffer &pairingBuff;
    EspNowManager &network;
    EventBus &eventBus;

    std::vector<PendingPairing> pendingPairs;

    void handleSettingsReport(const esp_now_recv_info_t *info, const SettingsReportPacket &pkt)
    {
        MacAddress mac(info->src_addr);

        std::optional<Device> devOpt = registry.getByMac(mac);
        if (!devOpt.has_value())
        {
            LOG_W(TAG, "Received settings from unknown device: %s", mac.toString().c_str());
            return;
        }

        removePending(mac);

        Device dev = devOpt.value();
        dev.settings.reportIntervalSec = pkt.reportIntervalSec;
        dev.settings.isValid = true;
        dev.header.status = DeviceStatus::ACTIVE;

        if (!registry.save(dev))
        {
            LOG_E(TAG, "Failed to save device settings %s", mac.toString().c_str());
            return;
        }

        StartWorkModePacket startPkt;
        startPkt.timestamp = millis() / 1000;
        network.send(mac, startPkt);

        LOG_I(TAG, "Device %s successfully connected and transferred to WORK_MODE", mac.toString().c_str());

        JsonDocument doc = deviceToJson(dev);
        eventBus.publish(EventType::DEVICE_UPDATED, doc.as<JsonVariantConst>());
    }

    void removePending(const MacAddress& mac)
    {
        for (auto it = pendingPairs.begin(); it != pendingPairs.end(); ++it)
        {
            if (it->mac == mac)
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
                LOG_W(TAG, "Pairing timeout for device: %s", it->mac.toString().c_str());

                Device failedDev{};
                failedDev.header.mac = it->mac;
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

    PairResult pairDevice(const MacAddress& mac)
    {
        if (registry.getByMac(mac).has_value())
        {
            return PairResult::ALREADY_PAIRED;
        }

        const DiscoveredDevice *foundInBuff = nullptr;
        for (const auto &dev : pairingBuff.getList())
        {
            if (dev.device.header.mac ==  mac)
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
        newDevice.header.mac = foundInBuff->device.header.mac;
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
        pending.mac = mac;
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