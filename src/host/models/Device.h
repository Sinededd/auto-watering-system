#pragma once
#include <Arduino.h>
#include <Packets.h>

enum class DeviceStatus : uint8_t
{
    ACTIVE = 0,
    INACTIVE,
    BLOCKED
};

struct DeviceHeader
{
    uint8_t mac[6];
    DeviceType type;
    DeviceStatus status;
};

struct DeviceSettings
{
    bool isValid;
    //...
};


struct Device
{
    DeviceHeader header;
    DeviceSettings settings;
};


template <size_t MAX_DEVICES = 32>
struct DeviceListStorage {
    uint8_t count = 0;
    Device items[MAX_DEVICES];
};