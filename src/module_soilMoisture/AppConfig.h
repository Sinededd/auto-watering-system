#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Logger.h>
#include <Device.h>
#include "StorageManager.h"


#pragma pack(push, 1)

struct DeviceConfig
{
    DeviceSettings settings;
    DeviceInterface interface;
};

#pragma pack(pop)


class AppConfig
{
private:
    static constexpr const char *TAG = "AppConfig";
    static constexpr const char *NVS_KEY = "dev_cfg";
    static constexpr const char *FACTORY_FILE = "/config.json";

    inline static RTC_DATA_ATTR RtcCache<DeviceConfig> _rtcCache;
    
    inline static StorageManager _storage{"app_cfg"};
    inline static bool _initialized = false;

    static void safeStrncpy(char *dest, const char *src, size_t size)
    {
        if (src == nullptr) {
            dest[0] = '\0';
            return;
        }
        strncpy(dest, src, size - 1);
        dest[size - 1] = '\0';
    }

    // Reserve setting (LitteFS not connect)
    static void applyHardcodedDefaults(DeviceConfig &cfg)
    {
        LOG_W(TAG, "Applying hardcoded fallback defaults");
        memset(&cfg, 0, sizeof(DeviceConfig));

        // Settings
        cfg.settings.timeToSleepSec = 60;
        cfg.settings.wifiPower = 34;
        cfg.settings.maxSkippedBoots = 3;

        // Interface
        cfg.interface.metricCount = 1;
        auto &m = cfg.interface.metrics[0];
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

    // Read factory /config.json from LittleFS
    static bool loadFactoryFromLittleFS(DeviceConfig &cfg)
    {
        if (!LittleFS.begin(true)) {
            LOG_E(TAG, "Failed to mount LittleFS");
            return false;
        }

        if (!LittleFS.exists(FACTORY_FILE)) {
            LOG_W(TAG, "Config file '%s' not found on LittleFS", FACTORY_FILE);
            return false;
        }

        File file = LittleFS.open(FACTORY_FILE, "r");
        if (!file) {
            LOG_E(TAG, "Failed to open config file for reading");
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            LOG_E(TAG, "JSON parse error: %s", error.c_str());
            return false;
        }

        memset(&cfg, 0, sizeof(DeviceConfig));

        // 1. Settings
        JsonObject sObj = doc["settings"];
        cfg.settings.timeToSleepSec = sObj["timeToSleepSec"] | 60;
        cfg.settings.wifiPower = sObj["wifiPower"] | 34;
        cfg.settings.maxSkippedBoots = sObj["maxSkippedBoots"] | 3;

        // 2. Interface
        JsonObject iObj = doc["interface"];
        uint8_t count = iObj["metricCount"] | 0;
        if (count > 3) count = 3;
        cfg.interface.metricCount = count;

        JsonArray metricsArr = iObj["metrics"];
        for (uint8_t i = 0; i < count; ++i)
        {
            JsonObject mObj = metricsArr[i];
            auto &m = cfg.interface.metrics[i];

            m.type = mObj["type"] | 0;
            safeStrncpy(m.label, mObj["label"] | "Sensor", sizeof(m.label));
            safeStrncpy(m.unit, mObj["unit"] | "", sizeof(m.unit));
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

        LOG_I(TAG, "Factory defaults loaded from LittleFS");
        return true;
    }

public:
    /// @brief Initial from start
    static bool begin()
    {
        if (_initialized) return true;

        // Autoload: RTC SRAM -> NVS Flash
        auto loaded = _storage.load<DeviceConfig>(NVS_KEY, &_rtcCache);

        if (loaded.has_value())
        {
            LOG_I(TAG, "Config loaded from %s", _rtcCache.isValid() ? "RTC RAM" : "NVS Flash");
            _initialized = true;
            return true;
        }

        LOG_W(TAG, "Config not found. Initializing factory defaults...");
        DeviceConfig factoryCfg{};

        if (!loadFactoryFromLittleFS(factoryCfg)) {
            applyHardcodedDefaults(factoryCfg);
        }

        update(factoryCfg);

        _initialized = true;
        return true;
    }

    /// @brief Get all config device (settings + interface).
    static const DeviceConfig& get() { return _rtcCache.data; }

    /// @brief Settings of device.
    static const DeviceSettings& settings() { return _rtcCache.data.settings; }

    /// @brief Interface of device.
    static const DeviceInterface& interface() { return _rtcCache.data.interface; }

    static uint32_t sleepTime() { return _rtcCache.data.settings.timeToSleepSec; }
    static uint8_t wifiPower() { return _rtcCache.data.settings.wifiPower; }
    static uint8_t maxSkippedBoots() { return _rtcCache.data.settings.maxSkippedBoots; }


    /// @brief Save all new config (write in NVS Flash and RTC RAM)
    static bool update(const DeviceConfig &newCfg)
    {
        bool ok = _storage.save<DeviceConfig>(NVS_KEY, newCfg, &_rtcCache);
        if (ok) {
            LOG_I(TAG, "DeviceConfig updated in NVS & RTC RAM");
        } else {
            LOG_E(TAG, "Failed to update DeviceConfig in NVS");
        }
        return ok;
    }

    /// @brief Update only setting of device (DeviceSettings)
    static bool updateSettings(const DeviceSettings &newSettings)
    {
        DeviceConfig updated = _rtcCache.data;
        updated.settings = newSettings;
        return update(updated);
    }

    /// @brief Update only interface of device (DeviceInterface)
    static bool updateInterface(const DeviceInterface &newInterface)
    {
        DeviceConfig updated = _rtcCache.data;
        updated.interface = newInterface;
        return update(updated);
    }

    /// @brief Reset to factory setting from LittleFS
    static bool resetToFactory()
    {
        LOG_W(TAG, "Factory reset requested!");
        _storage.remove<DeviceConfig>(NVS_KEY, &_rtcCache);
        
        DeviceConfig factoryCfg{};
        if (!loadFactoryFromLittleFS(factoryCfg)) {
            applyHardcodedDefaults(factoryCfg);
        }
        return update(factoryCfg);
    }
};