#pragma once

#include <ArduinoJson.h>
#include <MacAddress.h>

#include "ICommand.h"
#include "host/controllers/CalibrationController.h"

class CalibrationCommand : public ICommand
{
private:
    CalibrationController &controller;

    String helpCommand() const
    {
        return "Использование:\n"
               "  calibration start <MAC> [metricIndex]\n"
               "  calibration status <MAC>\n"
               "  calibration capture <MAC> <dry|wet>\n"
               "  calibration finish <MAC>\n"
               "  calibration cancel <MAC>";
    }

    bool parseMac(const JsonVariantConst &payload, MacAddress &mac, String &error) const
    {
        String macString = payload["mac"] | payload["args"][1] | "";
        if (macString.length() == 0)
        {
            error = "Укажите MAC-адрес датчика";
            return false;
        }

        if (!MacAddress::fromString(macString.c_str(), mac))
        {
            error = "Неверно указан MAC-адрес";
            return false;
        }

        return true;
    }

public:
    explicit CalibrationCommand(CalibrationController &controller)
        : controller(controller) {}

    String getName() const override { return "calibration"; }
    String getDescription() const override
    {
        return "calibration [start|status|capture|finish|cancel] — Калибровка датчика";
    }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";
        if (action == "" || action == "help")
        {
            return {true, helpCommand()};
        }

        MacAddress mac;
        String error;
        if (!parseMac(payload, mac, error))
        {
            return {false, error};
        }

        CalibrationOperationResult result{false, "Неизвестное действие"};

        if (action == "start")
        {
            uint8_t metricIndex = payload["metricIndex"] | payload["args"][2] | 0;
            result = controller.start(mac, metricIndex);
        }
        else if (action == "status")
        {
            result = controller.writeStatus(
                mac,
                outData["calibration"].to<JsonObject>());
        }
        else if (action == "capture")
        {
            String point = payload["point"] | payload["args"][2] | "";
            point.toLowerCase();
            result = controller.capture(mac, point);
        }
        else if (action == "finish")
        {
            result = controller.finish(mac);
        }
        else if (action == "cancel")
        {
            result = controller.cancel(mac);
        }
        else
        {
            return {false, "Неизвестное действие. Используйте 'calibration help'"};
        }

        if (action != "status")
        {
            controller.writeStatus(mac, outData["calibration"].to<JsonObject>());
        }

        return {result.success, result.message};
    }
};
