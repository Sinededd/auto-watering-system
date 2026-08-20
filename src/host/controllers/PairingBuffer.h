#pragma once
#include <Arduino.h>
#include <vector>
#include <Logger.h>
#include <Converter.h>
#include "host/events/EventBus.h"

struct DiscoveredDevice {
    uint8_t mac[6];
    DeviceType type;
    uint32_t lastSeenMs;
};

class PairingBuffer {
private:
static constexpr const char *TAG = "PairingBuffer";
    std::vector<DiscoveredDevice> queue;
    const uint32_t TTL_MS = 10000;
    EventBus& eventBus;

public:
    explicit PairingBuffer(EventBus& bus) : eventBus(bus) {}

    void onPairRequest(const uint8_t* mac, DeviceType type) {
        uint32_t now = millis();

        for (auto& dev : queue) {
            if (memcmp(dev.mac, mac, 6) == 0) {
                dev.lastSeenMs = now;
                return;
            }
        }

        DiscoveredDevice newDev;
        memcpy(newDev.mac, mac, 6);
        newDev.type = type;
        newDev.lastSeenMs = now;
        
        queue.push_back(newDev);
        LOG_I(TAG, "New device found nearby: %s (%s)",  macToString(newDev.mac).c_str(), deviceTypeToRussian(newDev.type). c_str());

        JsonDocument doc;
        doc["mac"] = macToString(newDev.mac);
        doc["type"] = static_cast<int>(newDev.type);
        eventBus.publish(EventType::DEVICE_DISCOVERED, doc.as<JsonVariantConst>());
    }

    void update() {
        uint32_t now = millis();
        
        for (auto it = queue.begin(); it != queue.end(); ) {
            if (now - it->lastSeenMs > TTL_MS) {
                LOG_I(TAG, "Device out of reach: %s", macToString(it->mac).c_str());

                JsonDocument doc;
                doc["mac"] = macToString(it->mac);
                eventBus.publish(EventType::DEVICE_LOST, doc.as<JsonVariantConst>());

                it = queue.erase(it);
            } else {
                ++it;
            }
        }
    }

    void remove(const uint8_t* mac) {
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (memcmp(it->mac, mac, 6) == 0) {
                queue.erase(it);
                break;
            }
        }
    }

    const std::vector<DiscoveredDevice>& getList() const {
        return queue;
    }
};