#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <functional>
#include <algorithm>

enum class EventType {
    DEVICE_DISCOVERED,
    DEVICE_UPDATED,
    DEVICE_LOST,
    DEVICE_TELEMETRY
};

class EventBus {
public:
    using Handler = std::function<void(const JsonVariantConst&)>;
    using SubscriptionId = uint32_t;

private:
    struct Listener {
        SubscriptionId id;
        Handler handler;
    };

    std::map<EventType, std::vector<Listener>> listeners;
    SubscriptionId nextId = 1;

public:
    SubscriptionId subscribe(EventType type, Handler handler) {
        SubscriptionId id = nextId++;
        listeners[type].push_back({id, handler});
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        if (id == 0) return;

        for (auto& pair : listeners) {
            auto& list = pair.second;
            
            auto it = std::remove_if(list.begin(), list.end(),
                [id](const Listener& listener) { return listener.id == id; });

            if (it != list.end()) {
                list.erase(it, list.end());
                return;
            }
        }
    }

    void publish(EventType type, const JsonVariantConst& payload = {}) {
        auto it = listeners.find(type);
        if (it != listeners.end()) {
            for (const auto& listener : it->second) {
                if (listener.handler) listener.handler(payload);
            }
        }
    }
};