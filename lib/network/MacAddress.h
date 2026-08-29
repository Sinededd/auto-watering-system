#pragma once

#include <Arduino.h>
#include <string>
#include <cstring>
#include <cstdio>

struct MacAddress {
    uint8_t bytes[6];

    MacAddress() = default;
    
    MacAddress(const uint8_t *ptr) {
        if (ptr) std::memcpy(bytes, ptr, 6);
    }

    bool operator==(const MacAddress &other) const {
        return std::memcmp(bytes, other.bytes, 6) == 0;
    }

    std::string toString() const {
        char buffer[18];
        std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
        return std::string(buffer);
    }

    String toArduinoString() const {
        return String(toString().c_str());
    }

    static bool fromString(const std::string &str, MacAddress &outMac) {
        if (str.length() != 17) return false;

        int values[6];
        if (std::sscanf(str.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
                        &values[0], &values[1], &values[2],
                        &values[3], &values[4], &values[5]) != 6) {
            return false;
        }

        for (int i = 0; i < 6; i++) {
            outMac.bytes[i] = static_cast<uint8_t>(values[i]);
        }
        return true;
    }
};

namespace std 
{
    template <>
    struct hash<MacAddress>
    {
        std::size_t operator()(const MacAddress &mac) const noexcept
        {
            // Упаковываем первые 4 байта и 2 байта в 32-битное число для быстрого хэша
            uint32_t hash = (mac.bytes[0] << 24) | (mac.bytes[1] << 16) | 
                            (mac.bytes[2] << 8)  | mac.bytes[3];
            uint16_t tail = (mac.bytes[4] << 8)  | mac.bytes[5];
            return std::hash<uint32_t>{}(hash) ^ (std::hash<uint16_t>{}(tail) << 1);
        }
    };
}