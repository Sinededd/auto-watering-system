#pragma once
#include <Arduino.h>
#include <vector>
#include <Logger.h>
#include <Converter.h>
#include "host/events/EventBus.h"
#include "host/models/Device.h"

struct DiscoveredDevice
{
    Device device;
    uint32_t lastSeenMs;
};

class PairingBuffer
{
private:
    static constexpr const char *TAG = "PairingBuffer";
    std::vector<DiscoveredDevice> queue;
    const uint32_t TTL_MS = 10000;
    EventBus &eventBus;
    DeviceRegistry &registry;

public:
    explicit PairingBuffer(EventBus &bus, DeviceRegistry &registry) : eventBus(bus), registry(registry)  {}

    void onPairRequest(const uint8_t *mac, DeviceType type)
    {
        if (registry.getByMac(mac).has_value())
        {
            return;
        }

        uint32_t now = millis();

        for (auto &item : queue)
        {
            if (memcmp(item.device.header.mac, mac, 6) == 0)
            {
                item.lastSeenMs = now;
                return;
            }
        }

        DiscoveredDevice item;
        memcpy(item.device.header.mac, mac, 6);
        item.device.header.type = type;
        item.device.header.status = DeviceStatus::UNPAIRED;
        item.device.settings.isValid = false;
        item.lastSeenMs = now;

        queue.push_back(item);
        LOG_I(TAG, "New device found nearby: %s (%s)",
              macToString(item.device.header.mac).c_str(),
              deviceTypeToRussian(item.device.header.type).c_str());

        JsonDocument doc = deviceToJson(item.device);
        eventBus.publish(EventType::DEVICE_DISCOVERED, doc.as<JsonVariantConst>());
    }

    void update()
    {
        uint32_t now = millis();

        for (auto it = queue.begin(); it != queue.end();)
        {
            if (now - it->lastSeenMs > TTL_MS)
            {
                LOG_I(TAG, "Device out of reach: %s", macToString(it->device.header.mac).c_str());

                JsonDocument doc = deviceToJson(it->device);
                eventBus.publish(EventType::DEVICE_LOST, doc.as<JsonVariantConst>());

                it = queue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void remove(const uint8_t *mac)
    {
        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            if (memcmp(it->device.header.mac, mac, 6) == 0)
            {
                queue.erase(it);
                break;
            }
        }
    }

    const std::vector<DiscoveredDevice> &getList() const
    {
        return queue;
    }
};