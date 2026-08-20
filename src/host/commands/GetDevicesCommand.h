#pragma once
#include "ICommand.h"

class GetDevicesCommand : public ICommand
{
public:
    String getName() const override { return "device.list"; }
    String getDescription() const override { return "Список устройств"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        JsonArray arr = outData["devices"].to<JsonArray>();
        arr.add("AA:BB:CC:DD:EE:FF");

        return {true, "Список сформирован"};
    }
};