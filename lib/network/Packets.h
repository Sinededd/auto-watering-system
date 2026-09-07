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
    TELEMETRY_REPORT
};

struct PacketHeader
{
    PacketType type;
};

// Sensor packets

struct PairRequestPacket
{
    PacketHeader packet = {PacketType::PAIR_REQUEST};
    char type[20];

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

// host packets

struct PairAcceptPacket
{
    PacketHeader packet = {PacketType::PAIR_ACCEPT};
};


struct StartWorkModePacket
{
    PacketHeader packet = {PacketType::START_WORK_MODE};
};

#pragma pack(pop)