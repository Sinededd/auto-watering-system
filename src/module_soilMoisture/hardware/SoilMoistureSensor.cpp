#include "SoilMoistureSensor.h"
#include <Logger.h>

SoilMoistureSensor::SoilMoistureSensor(uint8_t powerPin, uint8_t analogPin, int dryMilliVolts, int wetMilliVolts)
    : _powerPin(powerPin),
      _analogPin(analogPin),
      _dryMilliVolts(dryMilliVolts),
      _wetMilliVolts(wetMilliVolts),
      _isPowered(false) {}

void SoilMoistureSensor::powerOn()
{
    if (!_isPowered)
    {
        LOG_D(TAG, "Powering ON sensor (Power PIN: %u, ADC PIN: %u)", _powerPin, _analogPin);
        pinMode(_powerPin, OUTPUT);
        pinMode(_analogPin, INPUT);
        digitalWrite(_powerPin, HIGH);
        _isPowered = true;
        delay(400);
    }
}

void SoilMoistureSensor::powerOff()
{
    if (_isPowered)
    {
        LOG_D(TAG, "Powering OFF sensor");
        digitalWrite(_powerPin, LOW);
        gpio_reset_pin(static_cast<gpio_num_t>(_powerPin));
        gpio_reset_pin(static_cast<gpio_num_t>(_analogPin));
        _isPowered = false;
    }
}

int SoilMoistureSensor::readAveragedMilliVolts(uint8_t samples, uint8_t delayMs)
{
    long totalMilliVolts = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
        totalMilliVolts += analogReadMilliVolts(_analogPin);
        delay(delayMs);
    }
    return static_cast<int>(totalMilliVolts / samples);
}

int SoilMoistureSensor::readMilliVolts()
{
    bool wasOff = !_isPowered;
    if (wasOff)
    {
        powerOn();
    }

    int mv = readAveragedMilliVolts(5, 5);

    if (wasOff)
    {
        powerOff();
    }

    return mv;
}

float SoilMoistureSensor::readPercentage()
{
    int mv = readMilliVolts();
    int moisturePercent = map(mv, _dryMilliVolts, _wetMilliVolts, 0, 100);
    moisturePercent = constrain(moisturePercent, 0, 100);

    LOG_I(TAG, "Moisture read: %d%% (%d mV)", moisturePercent, mv);
    return static_cast<float>(moisturePercent);
}

void SoilMoistureSensor::startContinuous()
{
    LOG_I(TAG, "Starting continuous measurement mode");
    powerOn();
}

void SoilMoistureSensor::stop()
{
    LOG_I(TAG, "Stopping sensor measurement");
    powerOff();
}

void SoilMoistureSensor::setCalibration(int dryMilliVolts, int wetMilliVolts)
{
    _dryMilliVolts = dryMilliVolts;
    _wetMilliVolts = wetMilliVolts;
    LOG_D(TAG, "Calibration updated: Dry=%d mV, Wet=%d mV", _dryMilliVolts, _wetMilliVolts);
}