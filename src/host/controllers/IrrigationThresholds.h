#pragma once
#include <Arduino.h>
#include <unordered_map>
#include <string>
#include <optional>
#include <StorageManager.h>
#include <Logger.h>
#include <Device.h>

enum class SensorPickMode : uint8_t
{
    AVERAGE = 0,
    MIN,
    MAX
};

struct Thresholds
{
    SensorPickMode mode = SensorPickMode::AVERAGE;
    float minThreshold = 0.0f;
    float maxThreshold = 0.0f;
};

class IrrigationThresholds
{
private:
    static constexpr const char *TAG = "IRRIGATION_THRESHOLDS";

    StorageManager storage;
    std::unordered_map<std::string, Thresholds> thresholdsOfSensorTypes;

public:
    IrrigationThresholds() : storage("irr_thresh") {}

    void begin()
    {
        LOG_I(TAG, "Loading all thresholds into RAM...");
        thresholdsOfSensorTypes.clear();

        std::vector<std::string> keys = storage.getAllKeys();
        for (const auto &key : keys)
        {
            std::optional<Thresholds> thresholds = storage.load<Thresholds>(key.c_str());
            if (thresholds.has_value())
            {
                thresholdsOfSensorTypes[key] = thresholds.value();
                LOG_D(TAG, "Loaded thresholds for '%s' into RAM", key.c_str());
            }
        }
        LOG_I(TAG, "Loaded %u sensor threshold sets into RAM", (unsigned)thresholdsOfSensorTypes.size());
    }

    std::optional<Thresholds> getThresholds(const std::string &type)
    {
        if (type.empty())
            return std::nullopt;

        auto it = thresholdsOfSensorTypes.find(type);
        if (it != thresholdsOfSensorTypes.end())
        {
            return it->second;
        }
        // begin() loads NVS once; a missing type must not trigger a flash lookup
        // on every irrigation evaluation.
        return std::nullopt;
    }

    bool addThreshold(const std::string &type, const Thresholds &thresholds)
    {
        if (type.empty()) return false;
        LOG_I(TAG, "Saving thresholds for %s: min (%.2f), max (%.2f), mode (%d)", 
              type.c_str(), thresholds.minThreshold, thresholds.maxThreshold, static_cast<int>(thresholds.mode));
        
        thresholdsOfSensorTypes[type] = thresholds;
        return storage.save(type.c_str(), thresholds);
    }

    /// Автоматическое создание порога на основе границ метрики (minVal...maxVal)
    void autoCreateFromMetric(const std::string &key, const MetricDescriptor &metric)
    {
        if (key.empty()) return;
        if (getThresholds(key).has_value())
        {
            LOG_D(TAG, "Thresholds for '%s' already exist. Skipping auto-creation.", key.c_str());
            return;
        }

        float range = metric.maxVal - metric.minVal;
        
        // Значения по умолчанию: полив включается при 30% и выключается при 70% диапазона
        Thresholds autoThresh;
        autoThresh.mode = SensorPickMode::AVERAGE;
        autoThresh.minThreshold = metric.minVal + (range * 0.30f);
        autoThresh.maxThreshold = metric.minVal + (range * 0.70f);

        LOG_I(TAG, "Auto-generating threshold for '%s' (Range: %.1f..%.1f) -> Min: %.2f, Max: %.2f",
              key.c_str(), metric.minVal, metric.maxVal, autoThresh.minThreshold, autoThresh.maxThreshold);

        addThreshold(key, autoThresh);
    }

    bool removeThreshold(const std::string &type)
    {
        if (type.empty()) return false;
        thresholdsOfSensorTypes.erase(type);
        return storage.remove(type.c_str());
    }

    const std::unordered_map<std::string, Thresholds>& getAllThresholds() const
    {
        return thresholdsOfSensorTypes;
    }
};
