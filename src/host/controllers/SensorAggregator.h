#pragma once

#include <Arduino.h>
#include <unordered_map>
#include <vector>
#include <numeric>
#include <algorithm>
#include <optional>
#include <MacAddress.h>
#include "IrrigationThresholds.h"
#include "Packets.h"
#include "host/registry/DeviceRegistry.h"


struct SensorReading
{
    uint8_t count = 0;
    float values[3] = {0.0f, 0.0f, 0.0f};
    uint32_t timestampMs = 0;
};

class SensorAggregator
{
private:
    static constexpr const char *TAG = "SENSOR_AGGREGATOR";

    DeviceRegistry &deviceRegistry;
    
    uint32_t defaultDataTimeoutMs = 300000;
    std::unordered_map<std::string, std::unordered_map<MacAddress, SensorReading>> sensorData;

    /// @brief Dynamic timeout calculation considering sleep interval and skipped packets
    uint32_t getTimeoutForDevice(const MacAddress &mac) const
    {
        Device* device = deviceRegistry.findDevice(mac);
        if (device && device->settings.timeToSleepSec > 0)
        {
            uint8_t allowedMisses = 1;

            uint32_t totalCycles = 1 + allowedMisses;
            
            constexpr uint32_t MARGIN_MS = 10000;

            return (device->settings.timeToSleepSec * totalCycles * 1000) + MARGIN_MS;
        }

        return defaultDataTimeoutMs;
    }

    /// @brief Method for checking data freshness
    bool isReadingFresh(const SensorReading &reading, const MacAddress &mac, uint32_t now) const
    {
        uint32_t timeoutMs = getTimeoutForDevice(mac);
        return (now - reading.timestampMs <= timeoutMs);
    }

public:
    explicit SensorAggregator(DeviceRegistry &deviceRegistry) 
        : deviceRegistry(deviceRegistry) {}

    /// @brief Get data from TelemetryPacket
    void addReading(const std::string &type, const MacAddress &mac, const TelemetryPacket &pkt)
    {
        SensorReading reading;
        reading.count = (pkt.count > 3) ? 3 : pkt.count;
        reading.timestampMs = millis();
        for (uint8_t i = 0; i < reading.count; ++i)
        {
            reading.values[i] = pkt.values[i];
        }
        sensorData[type][mac] = reading;
    }

    /// @brief Get data as an array of values
    void addReading(const std::string &type, const MacAddress &mac, const float *values, uint8_t count)
    {
        SensorReading reading;
        reading.count = (count > 3) ? 3 : count;
        reading.timestampMs = millis();
        for (uint8_t i = 0; i < reading.count; ++i)
        {
            reading.values[i] = values[i];
        }
        sensorData[type][mac] = reading;
    }

    /// @brief Get a single value (for compatibility)
    void addReading(const std::string &type, const MacAddress &mac, float value)
    {
        float val = value;
        addReading(type, mac, &val, 1);
    }

    /// @brief Cleanup stale readings
    void cleanupStale()
    {
        uint32_t now = millis();
        for (auto typeIt = sensorData.begin(); typeIt != sensorData.end();)
        {
            auto &devices = typeIt->second;
            for (auto devIt = devices.begin(); devIt != devices.end();)
            {
                if (!isReadingFresh(devIt->second, devIt->first, now))
                {
                    devIt = devices.erase(devIt);
                }
                else
                {
                    ++devIt;
                }
            }

            if (devices.empty())
            {
                typeIt = sensorData.erase(typeIt);
            }
            else
            {
                ++typeIt;
            }
        }
    }

    /// @brief Get aggregated value
    /// @param type Type of device (dev.header.type)
    /// @param mode Mode (MIN, MAX, AVERAGE)
    /// @param metricIndex Index of metric (0..2). If -1, all values from packet are used (e.g., 3 humidity sensors)
    std::optional<float> getAggregatedValue(const std::string &type, SensorPickMode mode, int8_t metricIndex = -1) const
    {
        auto typeIt = sensorData.find(type);
        if (typeIt == sensorData.end() || typeIt->second.empty())
        {
            return std::nullopt;
        }

        uint32_t now = millis();
        std::vector<float> validValues;

        for (const auto &[mac, reading] : typeIt->second)
        {
            if (isReadingFresh(reading, mac, now))
            {
                if (metricIndex >= 0)
                {
                    if (metricIndex < reading.count)
                    {
                        validValues.push_back(reading.values[metricIndex]);
                    }
                }
                else
                {
                    for (uint8_t i = 0; i < reading.count; ++i)
                    {
                        validValues.push_back(reading.values[i]);
                    }
                }
            }
        }

        if (validValues.empty())
        {
            return std::nullopt;
        }

        switch (mode)
        {
        case SensorPickMode::MIN:
            return *std::min_element(validValues.begin(), validValues.end());

        case SensorPickMode::MAX:
            return *std::max_element(validValues.begin(), validValues.end());

        case SensorPickMode::AVERAGE:
        default:
            float sum = std::accumulate(validValues.begin(), validValues.end(), 0.0f);
            return sum / validValues.size();
        }
    }

    std::unordered_map<std::string, std::unordered_map<MacAddress, SensorReading>> getAllSensorData() { return sensorData; }
};