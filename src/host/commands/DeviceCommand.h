#pragma once
#include <ArduinoJson.h>
#include <MacAddress.h>
#include "ICommand.h"
#include "host/controllers/DeviceController.h"


class DeviceCommand : public ICommand
{
private:
    DeviceController &deviceController;

    String helpCommand() {
        return "Использование:\n"
               "  device list [all|pairing|saved] — список устройств\n"
               "  device connect <MAC> — подключить устройство";
    }

public:
    explicit DeviceCommand(
        DeviceController &deviceController
    ) : deviceController(deviceController) {}

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

            std::unordered_map<MacAddress, DeviceHeader> devices = deviceController.getPendingPairs();
            for (const auto &[mac, header] : devices)
            {
                JsonArray savedArr = outData["pairing"].to<JsonArray>();
                JsonObject obj = savedArr.add<JsonObject>();
                deviceHeaderToJson(header, obj);
            }

            return {true, "OK"};
        }

        return {false, "Неизвестное действие. Используйте 'device help'"};
    }
};