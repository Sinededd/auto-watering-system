#pragma once
#include <Arduino.h>
#include <Packets.h>

enum class DeviceStatus : uint8_t
{
    UNPAIRED = 0,
    CONNECTING,
    ACTIVE,
    INACTIVE,
    BLOCKED
};

struct DeviceHeader
{
    uint8_t mac[6];
    DeviceType type;
    DeviceStatus status;
};

struct DeviceSettings
{
    bool isValid = false;
    uint16_t reportIntervalSec = 60;
};

struct Device
{
    DeviceHeader header;
    DeviceSettings settings;
};

template <size_t MAX_DEVICES = 32>
struct DeviceListStorage
{
    uint8_t count = 0;
    Device items[MAX_DEVICES];
};

inline void deviceToJson(const Device &dev, JsonObject obj)
{
    obj["mac"] = macToString(dev.header.mac);
    obj["type"] = static_cast<int>(dev.header.type);
    obj["status"] = static_cast<int>(dev.header.status);

    JsonObject settings = obj["settings"].to<JsonObject>();
    settings["isValid"] = dev.settings.isValid;
    settings["reportIntervalSec"] = dev.settings.reportIntervalSec;
}

inline JsonDocument deviceToJson(const Device &dev)
{
    JsonDocument doc;
    deviceToJson(dev, doc.to<JsonObject>());
    return doc;
}