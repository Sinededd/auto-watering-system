#pragma once

#include <Arduino.h>
#include "driver/gpio.h"

class SoilMoistureSensor
{
private:
    static constexpr const char *TAG = "SoilMoisture";

    uint8_t _powerPin;
    uint8_t _analogPin;
    int _dryMilliVolts;
    int _wetMilliVolts;
    bool _isPowered;

    void powerOn();
    void powerOff();
    int readAveragedMilliVolts(uint8_t samples = 5, uint8_t delayMs = 5);

public:
    SoilMoistureSensor(uint8_t powerPin, uint8_t analogPin, int dryMilliVolts = 3100, int wetMilliVolts = 1400);

    /// @brief Read raw voltage in mV. Auto-manages power if not in continuous mode.
    int readMilliVolts();

    /// @brief Reads moisture percentage (0-100%). Auto-manages power if not in continuous mode.
    float readPercentage();

    /// @brief Enables continuous power mode for fast frequent polling.
    void startContinuous();

    /// @brief Powers off the sensor and releases GPIO pins.
    void stop();

    /// @brief Dynamic calibration update
    void setCalibration(int dryMilliVolts, int wetMilliVolts);

    /// @brief Returns current power state
    bool isPowered() const { return _isPowered; }
};