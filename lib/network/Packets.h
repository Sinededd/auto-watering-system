#pragma once
#include <Arduino.h>

inline constexpr uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum class DeviceType : uint8_t
{
    UNKNOWN = 0,
    HOST,
    SOIL_MOISTURE, // soil moisture
    AIR_THP        // air temperature and humidity
};

inline std::string deviceTypeToRussian(DeviceType type)
{
    switch (type)
    {
    case DeviceType::UNKNOWN:
        return "Неизвестный";
    case DeviceType::HOST:
        return "Главный модуль";
    case DeviceType::SOIL_MOISTURE:
        return "Влажность почвы";
    case DeviceType::AIR_THP:
        return "Влаж. и тем. воздуха";
    default:
        return std::to_string(static_cast<int>(type));
    }
}

enum class PacketType : uint8_t
{
    UNKNOWN = 0,
    PAIR_REQUEST,
    PAIR_ACCEPT,
    SETTINGS_REPORT,
    START_WORK_MODE,
    HELLO
};

struct __attribute__((packed)) PacketHeader
{
    PacketType type;
};

struct __attribute__((packed)) PairRequestPacket
{
    PacketHeader header = {PacketType::PAIR_REQUEST};
    uint8_t deviceMac[6];
    DeviceType deviceType;
};

struct __attribute__((packed)) PairAcceptPacket
{
    PacketHeader header = {PacketType::PAIR_ACCEPT};
};

struct __attribute__((packed)) SettingsReportPacket
{
    PacketHeader header = {PacketType::SETTINGS_REPORT};
    uint16_t reportIntervalSec;
};

struct __attribute__((packed)) StartWorkModePacket
{
    PacketHeader header = {PacketType::START_WORK_MODE};
    uint32_t timestamp;
};

struct __attribute__((packed)) HelloPacket
{
    PacketHeader header = {PacketType::HELLO};
    uint32_t timestamp;
};