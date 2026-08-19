#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct CommandResult {
    bool success;
    String message;
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual String getName() const = 0;
    virtual String getDescription() const = 0;

    virtual CommandResult execute(const JsonVariantConst& payload, JsonDocument& outData) = 0;
};