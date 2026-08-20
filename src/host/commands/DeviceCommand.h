#pragma once
#include <ArduinoJson.h>
#include <Converter.h>
#include "ICommand.h"
#include "host/controllers/PairingBuffer.h"
#include "host/controllers/DeviceRegistry.h"

class DeviceCommand : public ICommand
{
private:
    PairingBuffer &pairingBuffer;
    DeviceRegistry &deviceRegistry;

    String helpCommand() {
        return "Использование: device list [all|pairing|saved]";
    }

public:
    explicit DeviceCommand(DeviceRegistry &deviceRegistry, PairingBuffer &pairingBuffer) : 
        deviceRegistry(deviceRegistry), pairingBuffer(pairingBuffer) {}

    String getName() const override { return "device"; }
    String getDescription() const override { return "device [help|list] — Управление устройствами"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";
        String value = payload["filter"] | payload["args"][1] | "all";

        if (action == "help" || action == "") {
            return {true, helpCommand()};
        } 
        
        if (action == "list") {
            if (value == "pairing" || value == "all") {
                JsonArray pairingArr = outData["pairing"].to<JsonArray>();
                for (const auto &dev : pairingBuffer.getList()) {
                    JsonObject item = pairingArr.add<JsonObject>();
                    item["mac"] = macToString(dev.mac);
                    item["type"] = static_cast<int>(dev.type);
                    item["lastSeenAgoMs"] = millis() - dev.lastSeenMs;
                }
            }

            if (value == "saved" || value == "connected" || value == "all") {
                JsonArray savedArr = outData["saved"].to<JsonArray>();
                auto savedDevices = deviceRegistry.getAll();
                
                if (savedDevices.has_value()) {
                    for (uint8_t i = 0; i < savedDevices->count; ++i) {
                        const auto &dev = savedDevices->items[i];
                        JsonObject item = savedArr.add<JsonObject>();
                        item["mac"] = macToString(dev.header.mac);
                        item["type"] = static_cast<int>(dev.header.type);
                        item["status"] = static_cast<int>(dev.header.status);
                        item["isConfigured"] = dev.settings.isValid;
                    }
                }
            }

            return {true, "OK"};
        }

        return {false, "Неизвестное действие. Используйте 'device help'"};
    }
};