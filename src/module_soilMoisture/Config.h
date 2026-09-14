#pragma once
#include <Arduino.h>

namespace Config {

    // --- Purpose of pins GPIO ---
    namespace Pins {
        constexpr uint8_t LED_PIN = LED_BUILTIN;   // built-in LED
        constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_4;
        constexpr uint8_t SENSOR_POWER_PIN = 10;
        constexpr uint8_t SENSOR_ANALOG_PIN = 1;
    }
}
