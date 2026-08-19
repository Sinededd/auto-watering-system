# StorageManager

A lightweight C++ library for ESP32 that saves and loads data using **NVS Flash** with automatic **RTC SRAM caching** for Deep Sleep.

---

**Features**
* **Fast Reads:** Reads from RTC SRAM instantly (0 ms) during Deep Sleep cycles.
* **Type-Safe Templates:** Works with `int`, `float`, custom `struct`s, `enum`s, and arrays.
* **Safe Return Values:** Returns `std::optional<T>` to easily check if data exists.

---

**Important Rules**

The user is responsible for allocating cache memory in RTC SRAM.
For every NVS key, you **must allocate a separate variable**.
To do this, declare a static or global variable using the `RTC_DATA_ATTR` attribute:
`RTC_DATA_ATTR RtcCache<YOUR_TYPE> savedVariable;`


This library works with **trivially copyable data types** (raw byte structures):
* Primitive types (`int`, `float`, `uint8_t`, `bool`, etc.)
* Fixed-size C-style arrays (`uint8_t mac[6]`)
* Plain Old Data (POD) `struct`s

⚠️ **Do NOT use:** `std::string`, `std::vector`, dynamic memory, or pointers inside saved structures.

---

**Simple Example**

```cpp
#include <Arduino.h>
#include "StorageManager.h"

StorageManager storage("app_cfg");

// ⚠️ Important: Each NVS key requires its OWN UNIQUE variable in RTC SRAM!
RTC_DATA_ATTR RtcCache<uint8_t> rtcMode;
RTC_DATA_ATTR RtcCache<float>   rtcTemp;

void setup() {
    Serial.begin(115200);

    // 1. NVS + RTC Cache (Main mode: written to Flash, fast reads from RTC)
    storage.save("mode", (uint8_t)2, &rtcMode);
    auto mode = storage.load<uint8_t>("mode", &rtcMode);

    // 2. NVS Only (Rare settings: saves limited RTC memory)
    storage.save("volume", 80);
    auto vol = storage.load<int>("volume");

    // 3. RTC SRAM Only (Frequent data: 0 ms read/write, zero Flash wear)
    storage.saveRtc(36.6f, rtcTemp);
    auto temp = storage.loadRtc(rtcTemp);

    // 4. Cleanup (Invalidate RTC cache and/or remove key from NVS Flash)
    storage.invalidate(rtcMode);             // Invalidate RTC cache only
    storage.remove("mode", &rtcMode);       // Remove from NVS and invalidate RTC cache
}

void loop() {}