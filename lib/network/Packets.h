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
    SET_SETTINGS_RESPONSE, // Sensor -> Host
    WAKEUP_REPORT,                // Sensor -> Host
    CALIBRATION_START_REQUEST,    // Host -> Sensor
    CALIBRATION_START_RESPONSE,   // Sensor -> Host
    CALIBRATION_SAMPLE,           // Sensor -> Host
    CALIBRATION_FINISH_REQUEST,   // Host -> Sensor
    CALIBRATION_FINISH_RESPONSE,  // Sensor -> Host
    CALIBRATION_CANCEL_REQUEST,   // Host -> Sensor
    CALIBRATION_CANCEL_RESPONSE   // Sensor -> Host
};

enum class CalibrationError : uint8_t
{
    NONE = 0,
    INVALID_SESSION,
    INVALID_METRIC,
    INVALID_VALUES,
    SAVE_FAILED,
    BUSY
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

struct WakeupReportPacket
{
    PacketHeader packet = {PacketType::WAKEUP_REPORT};
    uint32_t remainingAwakeMs;
};

struct CalibrationStartResponsePacket
{
    PacketHeader packet = {PacketType::CALIBRATION_START_RESPONSE};
    uint32_t sessionId;
    bool success;
    CalibrationError error;
};

struct CalibrationSamplePacket
{
    PacketHeader packet = {PacketType::CALIBRATION_SAMPLE};
    uint32_t sessionId;
    uint32_t sequence;
    uint8_t metricIndex;
    uint16_t rawMilliVolts;
};

struct CalibrationFinishResponsePacket
{
    PacketHeader packet = {PacketType::CALIBRATION_FINISH_RESPONSE};
    uint32_t sessionId;
    bool success;
    CalibrationError error;
};

struct CalibrationCancelResponsePacket
{
    PacketHeader packet = {PacketType::CALIBRATION_CANCEL_RESPONSE};
    uint32_t sessionId;
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

struct CalibrationStartRequestPacket
{
    PacketHeader packet = {PacketType::CALIBRATION_START_REQUEST};
    uint32_t sessionId;
    uint8_t metricIndex;
};

struct CalibrationFinishRequestPacket
{
    PacketHeader packet = {PacketType::CALIBRATION_FINISH_REQUEST};
    uint32_t sessionId;
    uint8_t metricIndex;
    uint16_t dryMilliVolts;
    uint16_t wetMilliVolts;
};

struct CalibrationCancelRequestPacket
{
    PacketHeader packet = {PacketType::CALIBRATION_CANCEL_REQUEST};
    uint32_t sessionId;
};

#pragma pack(pop)
