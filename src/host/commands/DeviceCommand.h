#pragma once
#include <ArduinoJson.h>
#include <MacAddress.h>
#include "ICommand.h"
#include "host/controllers/DeviceController.h"
#include "host/controllers/PairingBuffer.h"
#include "host/controllers/DeviceRegistry.h"
#include "host/models/Device.h"

class DeviceCommand : public ICommand
{
private:
    DeviceController &deviceController;
    PairingBuffer &pairingBuffer;
    DeviceRegistry &deviceRegistry;

    String helpCommand() {
        return "Использование:\n"
               "  device list [all|pairing|saved] — список устройств\n"
               "  device connect <MAC> — подключить устройство";
    }

public:
    explicit DeviceCommand(
        DeviceController &deviceController,
        DeviceRegistry &deviceRegistry, 
        PairingBuffer &pairingBuffer
    ) : deviceController(deviceController), 
        deviceRegistry(deviceRegistry), 
        pairingBuffer(pairingBuffer) {}

    String getName() const override { return "device"; }
    String getDescription() const override { return "device [help|list|connect] — Управление устройствами"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";

        if (action == "help" || action == "") {
            return {true, helpCommand()};
        } 

        if (action == "connect" || action == "pair")
        {
            String macStr = payload["mac"] | payload["args"][1] | "";
            if (macStr.length() == 0) {
                return {false, "Укажите MAC-адрес (например: device connect 24:6F:28:AB:CD:12)"};
            }

            MacAddress mac;
            if (!MacAddress::fromString(macStr.c_str(), mac))
            {
                return {false, "Неверно указан MAC-адрес"};
            }

            PairResult res = deviceController.pairDevice(mac);
            return {(res == PairResult::SUCCESS), DeviceController::resultToString(res)};
        }

        if (action == "list") {
            String filter = payload["filter"] | payload["args"][1] | "all";

            if (filter == "pairing" || filter == "all") {
                JsonArray pairingArr = outData["pairing"].to<JsonArray>();
                for (const auto &item : pairingBuffer.getList()) {
                    JsonObject obj = pairingArr.add<JsonObject>();
                    deviceToJson(item.device, obj);
                    obj["lastSeenAgoMs"] = millis() - item.lastSeenMs;
                }
            }

            if (filter == "saved" || filter == "connected" || filter == "all") {
                JsonArray savedArr = outData["saved"].to<JsonArray>();
                auto savedDevices = deviceRegistry.getAll();

                if (savedDevices.has_value()) {
                    for (uint8_t i = 0; i < savedDevices->count; ++i) {
                        JsonObject obj = savedArr.add<JsonObject>();
                        deviceToJson(savedDevices->items[i], obj);
                    }
                }
            }

            return {true, "OK"};
        }

        return {false, "Неизвестное действие. Используйте 'device help'"};
    }
};