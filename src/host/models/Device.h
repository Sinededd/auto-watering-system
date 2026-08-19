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
    //...
};


struct Device
{
    DeviceHeader header;
    DeviceSettings settings;
};
