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
               "  device disconnect <MAC> — отключить и удалить устройство\n"
               "  device get_settings <MAC> — просмотреть текущие настройки устройства\n"
               "  device set_settings <MAC> <timeToSleepSec> [wifiPower] [maxSkippedBoots] — изменить настройки";
    }

public:
    explicit DeviceCommand(
        DeviceController &deviceController, DeviceRegistry &deviceRegistry)
        : deviceController(deviceController), deviceRegistry(deviceRegistry) {}

    String getName() const override { return "device"; }
    String getDescription() const override { return "device [help|list|connect|disconnect|get_settings|set_settings] — Управление устройствами"; }

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

        if (action == "get_settings" || action == "settings")
        {
            String macStr = payload["mac"] | payload["args"][1] | "";
            if (macStr.length() == 0)
            {
                return {false, "Укажите MAC-адрес (например: device get_settings 24:6F:28:AB:CD:12)"};
            }

            MacAddress mac;
            if (!MacAddress::fromString(macStr.c_str(), mac))
            {
                return {false, "Неверно указан MAC-адрес"};
            }

            const Device *dev = deviceRegistry.findDevice(mac);
            if (!dev)
            {
                return {false, "Устройство с таким MAC-адресом не найдено в реестре"};
            }

            JsonObject settingsObj = outData["settings"].to<JsonObject>();
            settingsObj["timeToSleepSec"] = dev->settings.timeToSleepSec;
            settingsObj["wifiPower"] = dev->settings.wifiPower;
            settingsObj["maxSkippedBoots"] = dev->settings.maxSkippedBoots;

            String msg = "Настройки " + mac.toArduinoString() + ":\n" +
                         "  Время сна: " + String(dev->settings.timeToSleepSec) + " сек\n" +
                         "  Мощность Wi-Fi: " + String(dev->settings.wifiPower) + "\n" +
                         "  Макс. пропусков перезагрузки: " + String(dev->settings.maxSkippedBoots);

            return {true, msg};
        }

        if (action == "set_settings" || action == "set_config")
        {
            String macStr = payload["mac"] | payload["args"][1] | "";
            if (macStr.length() == 0)
            {
                return {false, "Укажите MAC-адрес (например: device set_settings 24:6F:28:AB:CD:12 60 34 3)"};
            }

            MacAddress mac;
            if (!MacAddress::fromString(macStr.c_str(), mac))
            {
                return {false, "Неверно указан MAC-адрес"};
            }

            const Device *dev = deviceRegistry.findDevice(mac);
            if (!dev)
            {
                return {false, "Устройство не найдено в реестре хоста"};
            }

            // За основу берутся текущие настройки, чтобы при передаче не всех аргументов сохранялись старые значения
            DeviceSettings newSettings = dev->settings;

            if (!payload["timeToSleepSec"].isNull())
            {
                newSettings.timeToSleepSec = payload["timeToSleepSec"].as<uint32_t>();
            }
            else if (!payload["args"][2].isNull())
            {
                newSettings.timeToSleepSec = payload["args"][2].as<uint32_t>();
            }

            if (!payload["wifiPower"].isNull())
            {
                newSettings.wifiPower = payload["wifiPower"].as<uint8_t>();
            }
            else if (!payload["args"][3].isNull())
            {
                newSettings.wifiPower = payload["args"][3].as<uint8_t>();
            }

            if (!payload["maxSkippedBoots"].isNull())
            {
                newSettings.maxSkippedBoots = payload["maxSkippedBoots"].as<uint8_t>();
            }
            else if (!payload["args"][4].isNull())
            {
                newSettings.maxSkippedBoots = payload["args"][4].as<uint8_t>();
            }

            if (deviceController.requestSettingsUpdate(mac, newSettings))
            {
                return {true, "Запрос отправлен! Нажмите кнопку на датчике для применения настроек."};
            }

            return {false, "Не удалось запустить процесс обновления (возможно, операция уже выполняется)"};
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