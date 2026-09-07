#pragma once
#include <ArduinoJson.h>
#include "ICommand.h"
#include "host/controllers/IrrigationThresholds.h"

class ThresholdCommand : public ICommand
{
private:
    IrrigationThresholds &thresholds;

    String helpCommand()
    {
        return "Использование:\n"
               "  threshold list — список всех установленных порогов\n"
               "  threshold set <type> <min> <max> [mode] — установить порог (mode: avg, min, max)\n"
               "  threshold remove <type> — удалить порог для типа/метрики";
    }

    SensorPickMode parseMode(const String &modeStr)
    {
        String m = modeStr;
        m.toLowerCase();
        if (m == "min" || m == "1") return SensorPickMode::MIN;
        if (m == "max" || m == "2") return SensorPickMode::MAX;
        return SensorPickMode::AVERAGE;
    }

    String modeToString(SensorPickMode mode)
    {
        switch (mode)
        {
        case SensorPickMode::MIN: return "MIN";
        case SensorPickMode::MAX: return "MAX";
        case SensorPickMode::AVERAGE:
        default: return "AVERAGE";
        }
    }

public:
    explicit ThresholdCommand(IrrigationThresholds &thresholds)
        : thresholds(thresholds) {}

    String getName() const override { return "threshold"; }
    String getDescription() const override { return "threshold [help|list|set|remove] — Управление порогами полива"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";

        if (action == "help" || action == "")
        {
            return {true, helpCommand()};
        }

        // --- ЛИСТИНГ ПОРОГОВ ---
        if (action == "list")
        {
            const auto &all = thresholds.getAllThresholds();
            if (all.empty())
            {
                return {true, "Cписок порогов пуст"};
            }

            JsonArray arr = outData["thresholds"].to<JsonArray>();
            String msg = "Установленные пороги:\n";

            for (const auto &[type, thresh] : all)
            {
                JsonObject obj = arr.add<JsonObject>();
                obj["type"] = type.c_str();
                obj["min"] = thresh.minThreshold;
                obj["max"] = thresh.maxThreshold;
                obj["mode"] = modeToString(thresh.mode);

                msg += " - " + String(type.c_str()) +
                       ": min=" + String(thresh.minThreshold, 1) +
                       ", max=" + String(thresh.maxThreshold, 1) +
                       ", mode=" + modeToString(thresh.mode) + "\n";
            }
            return {true, msg};
        }

        // --- УСТАНОВКА ПОРОГА ---
        if (action == "set")
        {
            String type = payload["type"] | payload["args"][1] | "";
            if (type.length() == 0)
            {
                return {false, "Укажите тип или метку метрики (например: threshold set soil_mstr 30 70)"};
            }

            String minStr = payload["min"] | payload["args"][2] | "";
            String maxStr = payload["max"] | payload["args"][3] | "";

            if (minStr.length() == 0 || maxStr.length() == 0)
            {
                return {false, "Укажите min и max значения (например: threshold set soil_mstr 35 65 avg)"};
            }

            float minVal = minStr.toFloat();
            float maxVal = maxStr.toFloat();

            if (minVal >= maxVal)
            {
                return {false, "Ошибка: min должен быть строго меньше max"};
            }

            String modeStr = payload["mode"] | payload["args"][4] | "avg";
            SensorPickMode mode = parseMode(modeStr);

            Thresholds t;
            t.minThreshold = minVal;
            t.maxThreshold = maxVal;
            t.mode = mode;

            if (thresholds.addThreshold(type.c_str(), t))
            {
                return {true, "Порог для '" + type + "' сохранен (min: " +
                              String(minVal, 1) + ", max: " + String(maxVal, 1) + ", mode: " + modeToString(mode) + ")"};
            }
            return {false, "Ошибка при сохранении порога в NVS"};
        }

        // --- УДАЛЕНИЕ ПОРОГА ---
        if (action == "remove" || action == "del")
        {
            String type = payload["type"] | payload["args"][1] | "";
            if (type.length() == 0)
            {
                return {false, "Укажите тип (например: threshold remove soil_mstr)"};
            }

            if (thresholds.removeThreshold(type.c_str()))
            {
                return {true, "Порог для '" + type + "' успешно удален"};
            }
            return {false, "Не удалось удалить порог"};
        }

        return {false, "Неизвестное действие. Используйте 'threshold help'"};
    }
};