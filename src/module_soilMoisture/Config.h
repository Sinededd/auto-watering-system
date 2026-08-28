#pragma once
#include <Arduino.h>

namespace Config {

    // --- Purpose of pins GPIO ---
    namespace Pins {
        constexpr uint8_t LED_PIN = LED_BUILTIN;   // built-in LED
        constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_4;
    }
}