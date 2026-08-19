#pragma once
#include <Arduino.h>

constexpr uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


enum class DeviceType : uint8_t {
    UNKNOWN         = 0,
    SOIL_MOISTURE   = 1, // soil moisture
    AIR_THP         = 2, // air temperature and humidity
};


enum class PacketType : uint8_t
{
    UNKNOWN = 0,
    PAIR_REQUEST,
    HELLO
};

struct __attribute__((packed)) PacketHeader
{
    PacketType type;
};

struct __attribute__((packed)) PairRequestPacket
{
    PacketHeader header = {PacketType::PAIR_REQUEST};
    DeviceType deviceType;
};

struct __attribute__((packed)) HelloPacket
{
    PacketHeader header = {PacketType::HELLO};
    uint32_t timestamp;
};