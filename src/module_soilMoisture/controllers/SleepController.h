#pragma once
#include <Arduino.h>

class DeviceContext;

class SleepController {
private:
    static constexpr const char *TAG = "SLEEP_CONTROLLER";

    bool hasWakeUpButton = false;
    gpio_num_t buttonPin;

public:
    SleepController();
    SleepController(gpio_num_t wakeupButton);

    void wakeUp(DeviceContext &device);
    void goToSleep(uint32_t timeSec = 0);
};