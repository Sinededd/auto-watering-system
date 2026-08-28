#pragma once
#include <Arduino.h>

class DeviceContext;

class SleepController {
private:
    static constexpr const char *TAG = "SLEEP_CONTROLLER";

    bool hasWakeUpButton = false;
    gpio_num_t buttonPin;
    uint32_t defaultTimeSec;

public:
    SleepController(uint32_t defaultTimeSec = 300);
    SleepController(gpio_num_t wakeupButton, uint32_t defaultTimeSec = 300);

    void wakeUp(DeviceContext &device);
    void goToSleep(uint32_t timeSec = 0);
};