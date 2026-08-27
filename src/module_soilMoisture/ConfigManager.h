#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Packets.h"

class ConfigManager
{
private:
    static constexpr const char *TAG = "ConfigManager";
    static constexpr const char *CONFIG_FILE = "/config.json";

    static void safeStrncpy(char *dest, const char *src, size_t size)
    {
        strncpy(dest, src, size - 1);
        dest[size - 1] = '\0';
    }

public:
    static void loadDefaults(SettingsReportPacket &config)
    {
        LOG_I(TAG, "Loading default configuration");
        
        config.packet.type = PacketType::SETTINGS_REPORT;

        // Settings
        config.settings.timeToSleepSec = 60;
        config.settings.wifiPower = 34;
        config.settings.maxSkippedBoots = 3;

        // Interface
        config.interface.metricCount = 1;

        auto &m = config.interface.metrics[0];
        m.type = 1;
        safeStrncpy(m.label, "Soil moisture", sizeof(m.label));
        safeStrncpy(m.unit, "%", sizeof(m.unit));
        m.minVal = 0.0f;
        m.maxVal = 100.0f;
        m.grid = 0.1f;
        m.calib.pointsCount = 2;
        m.calib.rawValues[0] = 3100.0f;
        m.calib.rawValues[1] = 1400.0f;
    }

    static bool load(SettingsReportPacket &config)
    {
        if (!LittleFS.begin(true)) {
            LOG_E(TAG, "Failed to initialize LittleFS");
            loadDefaults(config);
            return false;
        }

        if (!LittleFS.exists(CONFIG_FILE)) {
            LOG_W(TAG, "Config file not found. Creating default file...");
            loadDefaults(config);
            save(config);
            return true;
        }

        File file = LittleFS.open(CONFIG_FILE, "r");
        if (!file) {
            LOG_E(TAG, "Failed to open config file for reading");
            loadDefaults(config);
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            LOG_E(TAG, "JSON parse error: %s", error.c_str());
            loadDefaults(config);
            return false;
        }

        config.packet.type = PacketType::SETTINGS_REPORT;

        JsonObject sObj = doc["settings"];
        config.settings.timeToSleepSec = sObj["timeToSleepSec"] | 60;
        config.settings.wifiPower = sObj["wifiPower"] | 34;
        config.settings.maxSkippedBoots = sObj["maxSkippedBoots"] | 3;

        JsonObject iObj = doc["interface"];
        uint8_t count = iObj["metricCount"] | 0;
        if (count > 3) count = 3;
        config.interface.metricCount = count;

        JsonArray metricsArr = iObj["metrics"];
        for (uint8_t i = 0; i < count; ++i)
        {
            JsonObject mObj = metricsArr[i];
            auto &m = config.interface.metrics[i];

            m.type = mObj["type"] | 0;

            const char *label = mObj["label"] | "Sensor";
            safeStrncpy(m.label, label, sizeof(m.label));

            const char *unit = mObj["unit"] | "";
            safeStrncpy(m.unit, unit, sizeof(m.unit));

            m.minVal = mObj["minVal"] | 0.0f;
            m.maxVal = mObj["maxVal"] | 100.0f;
            m.grid = mObj["grid"] | 1.0f;

            JsonObject calib = mObj["calib"];
            uint8_t pCount = calib["pointsCount"] | 0;
            if (pCount > 2) pCount = 2;
            m.calib.pointsCount = pCount;

            JsonArray rawArr = calib["rawValues"];
            for (uint8_t p = 0; p < pCount; ++p) {
                m.calib.rawValues[p] = rawArr[p] | 0.0f;
            }
        }

        LOG_I(TAG, "Configuration loaded successfully");
        return true;
    }

    static bool save(const SettingsReportPacket &config)
    {
        JsonDocument doc;

        JsonObject sObj = doc["settings"].to<JsonObject>();
        sObj["timeToSleepSec"] = config.settings.timeToSleepSec;
        sObj["wifiPower"] = config.settings.wifiPower;
        sObj["maxSkippedBoots"] = config.settings.maxSkippedBoots;

        JsonObject iObj = doc["interface"].to<JsonObject>();
        uint8_t count = config.interface.metricCount > 3 ? 3 : config.interface.metricCount;
        iObj["metricCount"] = count;

        JsonArray metricsArr = iObj["metrics"].to<JsonArray>();

        for (uint8_t i = 0; i < count; ++i) {
            const auto &m = config.interface.metrics[i];
            JsonObject mObj = metricsArr.add<JsonObject>();

            mObj["type"] = m.type;
            mObj["label"] = m.label;
            mObj["unit"] = m.unit;
            mObj["minVal"] = m.minVal;
            mObj["maxVal"] = m.maxVal;
            mObj["grid"] = m.grid;

            JsonObject calib = mObj["calib"].to<JsonObject>();
            uint8_t pCount = m.calib.pointsCount > 2 ? 2 : m.calib.pointsCount;
            calib["pointsCount"] = pCount;

            JsonArray rawArr = calib["rawValues"].to<JsonArray>();
            for (uint8_t p = 0; p < pCount; ++p) {
                rawArr.add(m.calib.rawValues[p]);
            }
        }

        File file = LittleFS.open(CONFIG_FILE, "w");
        if (!file) {
            LOG_E(TAG, "Failed to open config file for writing");
            return false;
        }

        serializeJsonPretty(doc, file);
        file.close();

        LOG_I(TAG, "Configuration saved successfully");
        return true;
    }
};