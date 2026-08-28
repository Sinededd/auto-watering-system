#include "SleepController.h"
#include <driver/gpio.h>
#include <Logger.h>
#include <module_soilMoisture/fsm/DeviceContext.h>
#include <module_soilMoisture/fsm/StartState.h>
#include <module_soilMoisture/fsm/WorkingState.h>

SleepController::SleepController(uint32_t defaultTimeSec)
{
    this->hasWakeUpButton = false;
    this->defaultTimeSec = defaultTimeSec;
}

SleepController::SleepController(gpio_num_t wakeupButton, uint32_t defaultTimeSec)
{
    this->hasWakeUpButton = true;
    this->buttonPin = wakeupButton;
    this->defaultTimeSec = defaultTimeSec;
}

void SleepController::wakeUp(DeviceContext &device)
{
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    if (wakeupReason == ESP_SLEEP_WAKEUP_EXT0)
    {
        LOG_I(TAG, "Woken up by BUTTON press!");
        device.getLed().turnOn();
    }
    else if (wakeupReason == ESP_SLEEP_WAKEUP_TIMER)
    {
        LOG_I(TAG, "Woken up by TIMER!");
        device.begin(std::make_unique<WorkingState>());
    }
    else
    {
        LOG_I(TAG, "Power on / Hard Reset");
        device.begin(std::make_unique<StartState>());
    }
}

void SleepController::goToSleep(uint32_t timeSec)
{
    if (hasWakeUpButton && buttonPin != GPIO_NUM_NC)
    {
        gpio_pullup_en(buttonPin);
        gpio_pulldown_dis(buttonPin);
        esp_deep_sleep_enable_gpio_wakeup(1ULL << buttonPin, ESP_GPIO_WAKEUP_GPIO_LOW);
    }

    if (timeSec == 0)
        timeSec = defaultTimeSec;

    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(timeSec) * 1000000ULL);

    LOG_I(TAG, "Going to Deep Sleep for %u sec", timeSec);
    Logger::flush();
    Serial.flush();

    esp_deep_sleep_start();
}
