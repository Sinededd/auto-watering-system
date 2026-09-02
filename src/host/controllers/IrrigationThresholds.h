#pragma once
#include <Arduino.h>
#include <unordered_map>
#include <string>
#include <optional>
#include <StorageManager.h>

enum class SensorPickMode : uint8_t
{
    AVERAGE = 0,
    MIN,
    MAX
};

struct Thresholds
{
    SensorPickMode mode;
    float minThreshold;
    float maxThreshold;
};

class IrrigationThresholds
{
private:
    static constexpr const char *TAG = "IRRIGATION_THRESHOLDS";

    StorageManager storage;
    std::unordered_map<std::string, Thresholds> thresholdsOfSensorTypes;

public:
    IrrigationThresholds() : storage("irr_thresh") {}

    /// @brief Load all thresholds from NVS into RAM. 
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
        auto it = thresholdsOfSensorTypes.find(type);
        if (it != thresholdsOfSensorTypes.end())
        {
            LOG_D(TAG, "Get thresholds for %s from RAM memory", type.c_str());
            return it->second;
        }

        std::optional<Thresholds> thresholds = storage.load<Thresholds>(type.c_str());
        if (thresholds.has_value())
        {
            LOG_D(TAG, "Get thresholds for %s from NVS memory", type.c_str());
            thresholdsOfSensorTypes[type] = thresholds.value();
        }
        else
        {
            LOG_E(TAG, "Error getting thresholds for %s", type.c_str());
        }

        return thresholds;
    }

    bool addThreshold(const std::string &type, const Thresholds &thresholds)
    {
        LOG_D(TAG, "Saving thresholds for %s: min (%f), max(%f), mode(%d)", 
              type.c_str(), thresholds.minThreshold, thresholds.maxThreshold, static_cast<int>(thresholds.mode));
        
        thresholdsOfSensorTypes[type] = thresholds;
        return storage.save(type.c_str(), thresholds);
    }

    bool removeThreshold(const std::string &type)
    {
        LOG_D(TAG, "Removing thresholds for %s", type.c_str());
        thresholdsOfSensorTypes.erase(type);
        return storage.remove(type.c_str());
    }
};