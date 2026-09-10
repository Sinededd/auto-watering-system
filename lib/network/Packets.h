#pragma once
#include <Arduino.h>
#include <string>
#include "Device.h"

inline constexpr uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#pragma pack(push, 1)

enum class PacketType : uint8_t
{
    UNKNOWN = 0,
    PAIR_REQUEST,
    PAIR_ACCEPT,
    SETTINGS_REPORT,
    START_WORK_MODE,
    TELEMETRY_REPORT,
    SET_SETTINGS_REQUEST,  // Host -> Sensor
    SET_SETTINGS_RESPONSE  // Sensor -> Host
};

struct PacketHeader
{
    PacketType type;
};

// Sensor packets

struct PairRequestPacket
{
    PacketHeader packet = {PacketType::PAIR_REQUEST};
    char type[12];

};

struct SettingsReportPacket
{
    PacketHeader packet = {PacketType::SETTINGS_REPORT};
    DeviceSettings settings;
    DeviceInterface interface;
};

struct TelemetryPacket
{
    PacketHeader packet = {PacketType::TELEMETRY_REPORT};
    uint8_t count;
    float values[3];
};

struct SetSettingsResponsePacket
{
    PacketHeader packet = {PacketType::SET_SETTINGS_RESPONSE};
    bool success;
};

// host packets

struct PairAcceptPacket
{
    PacketHeader packet = {PacketType::PAIR_ACCEPT};
};


struct StartWorkModePacket
{
    PacketHeader packet = {PacketType::START_WORK_MODE};
};

struct SetSettingsRequestPacket
{
    PacketHeader packet = {PacketType::SET_SETTINGS_REQUEST};
    DeviceSettings settings;
};

#pragma pack(pop)