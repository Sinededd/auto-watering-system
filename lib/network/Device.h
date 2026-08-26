#pragma once
#include <Arduino.h>
#include <Packets.h>
#include <MacAddress.h>
#include <ArduinoJson.h>

#pragma pack(push, 1)

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
    MacAddress mac;
    DeviceStatus status;
    char type[12];
    int8_t sigLevel;
    uint32_t lastSeenMs = 0;
};

struct DeviceSettings
{
    uint16_t reportIntervalSec;
    uint32_t timeToSleepSec;
    uint8_t wifiPower;
    uint8_t maxSkippedBoots;
};

struct CalibrationParams
{
    uint8_t pointsCount = 0;
    float rawValues[2];
};

struct MetricDescriptor
{
    uint8_t type;
    char label[24];
    char unit[8];
    float minVal;
    float maxVal;
    float grid;
    CalibrationParams calib;
};

struct DeviceInterface
{
    uint8_t metricCount;
    MetricDescriptor metrics[3];
};

struct Device
{
    DeviceHeader header;
    DeviceSettings settings;
    DeviceInterface interface;
};

#pragma pack(pop)

inline const char* deviceStatusToString(DeviceStatus status)
{
    switch (status)
    {
        case DeviceStatus::UNPAIRED:   return "Не привязан";
        case DeviceStatus::CONNECTING: return "Подключение";
        case DeviceStatus::ACTIVE:     return "Активен";
        case DeviceStatus::INACTIVE:   return "Неактивен";
        case DeviceStatus::BLOCKED:    return "Заблокирован";
        default:                       return "Неизвестно";
    }
}

inline void deviceToJson(const Device &dev, JsonObject obj)
{
    JsonObject header = obj["header"].to<JsonObject>();
    header["mac"] = dev.header.mac.toArduinoString();
    header["status"] = static_cast<uint8_t>(dev.header.status);
    header["statusStr"] = deviceStatusToString(dev.header.status);
    header["type"] = dev.header.type;
    header["sigLevel"] = dev.header.sigLevel;
    header["lastSeenMs"] = dev.header.lastSeenMs;
    
    uint32_t secondsAgo = (dev.header.lastSeenMs > 0) ? (millis() - dev.header.lastSeenMs) / 1000 : 0;
    header["lastSeenSecAgo"] = secondsAgo;

    JsonObject settings = obj["settings"].to<JsonObject>();
    settings["reportIntervalSec"] = dev.settings.reportIntervalSec;
    settings["timeToSleepSec"] = dev.settings.timeToSleepSec;
    settings["wifiPower"] = dev.settings.wifiPower;
    settings["maxSkippedBoots"] = dev.settings.maxSkippedBoots;

    JsonObject interfaceObj = obj["interface"].to<JsonObject>();
    interfaceObj["metricCount"] = dev.interface.metricCount;

    JsonArray metricsArr = interfaceObj["metrics"].to<JsonArray>();

    uint8_t count = (dev.interface.metricCount > 3) ? 3 : dev.interface.metricCount;

    for (uint8_t i = 0; i < count; ++i)
    {
        const auto &m = dev.interface.metrics[i];
        JsonObject mObj = metricsArr.add<JsonObject>();

        mObj["type"] = m.type;
        mObj["label"] = m.label;
        mObj["unit"] = m.unit;
        mObj["minVal"] = m.minVal;
        mObj["maxVal"] = m.maxVal;
        mObj["grid"] = m.grid;

        JsonObject calib = mObj["calib"].to<JsonObject>();
        calib["pointsCount"] = m.calib.pointsCount;

        if (m.calib.pointsCount > 0)
        {
            JsonArray rawArr = calib["rawValues"].to<JsonArray>();
            uint8_t points = (m.calib.pointsCount > 2) ? 2 : m.calib.pointsCount;
            for (uint8_t p = 0; p < points; ++p)
            {
                rawArr.add(m.calib.rawValues[p]);
            }
        }
    }
}

inline JsonDocument deviceToJson(const Device &dev)
{
    JsonDocument doc;
    deviceToJson(dev, doc.to<JsonObject>());
    return doc;
}

inline void deviceHeaderToJson(const DeviceHeader &header, JsonObject obj)
{
    obj["mac"] = header.mac.toArduinoString();
    obj["status"] = static_cast<uint8_t>(header.status);
    obj["statusStr"] = deviceStatusToString(header.status);
    obj["type"] = header.type;
    obj["sigLevel"] = header.sigLevel;
    obj["lastSeenMs"] = header.lastSeenMs;

    uint32_t secondsAgo = (header.lastSeenMs > 0) ? (millis() - header.lastSeenMs) / 1000 : 0;
    obj["lastSeenSecAgo"] = secondsAgo;
}

inline JsonDocument deviceHeaderToJson(const DeviceHeader &header)
{
    JsonDocument doc;
    deviceHeaderToJson(header, doc.to<JsonObject>());
    return doc;
}