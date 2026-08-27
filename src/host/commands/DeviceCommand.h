#pragma once
#include <ArduinoJson.h>
#include <MacAddress.h>
#include "ICommand.h"
#include "host/controllers/DeviceController.h"
#include "host/registry/DeviceRegistry.h"

class DeviceCommand : public ICommand
{
private:
    DeviceController &deviceController;
    DeviceRegistry &deviceRegistry;

    String helpCommand()
    {
        return "Использование:\n"
               "  device list [all|pairing|saved] — список устройств\n"
               "  device connect <MAC> — подключить устройство\n"
               "  device disconnect <MAC> — отключить и удалить устройство";
    }

public:
    explicit DeviceCommand(
        DeviceController &deviceController, DeviceRegistry &deviceRegistry)
        : deviceController(deviceController), deviceRegistry(deviceRegistry) {}

    String getName() const override { return "device"; }
    String getDescription() const override { return "device [help|list|connect|disconnect] — Управление устройствами"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";

        if (action == "help" || action == "")
        {
            return {true, helpCommand()};
        }

        if (action == "connect" || action == "pair")
        {
            String macStr = payload["mac"] | payload["args"][1] | "";
            if (macStr.length() == 0)
            {
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

        if (action == "disconnect" || action == "unpair" || action == "remove")
        {
            String macStr = payload["mac"] | payload["args"][1] | "";
            if (macStr.length() == 0)
            {
                return {false, "Укажите MAC-адрес (например: device disconnect 24:6F:28:AB:CD:12)"};
            }

            MacAddress mac;
            if (!MacAddress::fromString(macStr.c_str(), mac))
            {
                return {false, "Неверно указан MAC-адрес"};
            }

            if (deviceController.disconnectDevice(mac))
            {
                return {true, "Устройство успешно отключено и удалено"};
            }

            return {false, "Устройство с таким MAC-адресом не найдено"};
        }

        if (action == "list")
        {
            String filter = payload["filter"] | payload["args"][2] | "";

            if (filter == "" || filter == "all")
            {
                auto pendingDevices = deviceController.getPendingPairs();
                JsonArray pairingArr = outData["pairing"].to<JsonArray>();
                for (const auto &[mac, header] : pendingDevices)
                {
                    JsonObject obj = pairingArr.add<JsonObject>();
                    deviceHeaderToJson(header, obj);
                }

                const auto &connectedDevices = deviceRegistry.getAllDevices();
                JsonArray connectedArr = outData["connected"].to<JsonArray>();
                for (const auto &device : connectedDevices)
                {
                    JsonObject obj = connectedArr.add<JsonObject>();
                    deviceHeaderToJson(device.header, obj);
                }
            }

            return {true, "OK"};
        }

        return {false, "Неизвестное действие. Используйте 'device help'"};
    }
};