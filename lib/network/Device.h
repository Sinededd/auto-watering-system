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

enum class MetricType : uint8_t
{
    CUSTOM = 0,
    SOIL_MOISTURE,
    AIR_TEMPERATURE,
    AIR_HUMIDITY,
};

struct DeviceHeader
{
    MacAddress mac;
    uint8_t deviceType;
    DeviceStatus status;
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
    MetricType type;
    float minVal;
    float maxVal;
    float targetThreshold;
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

template <size_t MAX_DEVICES = 32>
struct DeviceListStorage
{
    uint8_t count = 0;
    Device items[MAX_DEVICES];
};

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

inline const char* metricTypeToString(MetricType type)
{
    switch (type)
    {
        case MetricType::SOIL_MOISTURE:   return "Влажность почвы";
        case MetricType::AIR_TEMPERATURE: return "Температура воздуха";
        case MetricType::AIR_HUMIDITY:    return "Влажность воздуха";
        case MetricType::CUSTOM:          
        default:                          return "Пользовательская";
    }
}

inline const char* metricTypeToUnit(MetricType type)
{
    switch (type)
    {
        case MetricType::SOIL_MOISTURE:   return "%";
        case MetricType::AIR_TEMPERATURE: return "°C";
        case MetricType::AIR_HUMIDITY:    return "%";
        case MetricType::CUSTOM:          
        default:                          return "";
    }
}

inline void deviceToJson(const Device &dev, JsonObject obj)
{
    JsonObject header = obj["header"].to<JsonObject>();
    header["mac"] = dev.header.mac.toArduinoString();
    header["type"] = dev.header.deviceType;
    header["status"] = static_cast<uint8_t>(dev.header.status);
    header["statusStr"] = deviceStatusToString(dev.header.status);

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

        mObj["type"] = static_cast<uint8_t>(m.type);
        mObj["label"] = metricTypeToString(m.type);
        mObj["unit"] = metricTypeToUnit(m.type);
        mObj["minVal"] = m.minVal;
        mObj["maxVal"] = m.maxVal;
        mObj["targetThreshold"] = m.targetThreshold;

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