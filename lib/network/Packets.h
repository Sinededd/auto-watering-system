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
    HELLO
};

struct PacketHeader
{
    PacketType type;
};

struct PairRequestPacket
{
    PacketHeader packet = {PacketType::PAIR_REQUEST};
    DeviceHeader header;
};

struct PairAcceptPacket
{
    PacketHeader packet = {PacketType::PAIR_ACCEPT};
};

struct SettingsReportPacket
{
    PacketHeader packet = {PacketType::SETTINGS_REPORT};
    Device deviceInfo;
};

struct StartWorkModePacket
{
    PacketHeader packet = {PacketType::START_WORK_MODE};
    uint32_t timestamp;
};

struct HelloPacket
{
    PacketHeader packet = {PacketType::HELLO};
    uint32_t timestamp;
};

#pragma pack(pop)