#pragma once

#include <ArduinoJson.h>
#include <MacAddress.h>
#include "ICommand.h"
#include <host/controllers/SensorAggregator.h>


class SensorCommand : public ICommand
{
private:
    SensorAggregator &aggregator;

    String helpCommand() const
    {
        return "Использование:\n"
               "  sensor list — показать текущие актуальные данные датчиков\n"
               "  sensor calc <type> [avg|min|max] [metricIndex] — рассчитать агрегированное значение\n"
               "  sensor cleanup — принудительно очистить устаревшие записи";
    }

    SensorPickMode parseMode(const String &modeStr) const
    {
        String str = modeStr;
        str.toLowerCase();
        if (str == "min") return SensorPickMode::MIN;
        if (str == "max") return SensorPickMode::MAX;
        return SensorPickMode::AVERAGE;
    }

public:
    explicit SensorCommand(SensorAggregator &aggregator)
        : aggregator(aggregator) {}

    String getName() const override { return "sensor"; }
    String getDescription() const override { return "sensor [help|list|calc|cleanup] — Просмотр и агрегация данных датчиков"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";

        if (action == "help" || action == "")
        {
            return {true, helpCommand()};
        }

        if (action == "list" || action == "get")
        {
            auto allData = aggregator.getAllsensorData();
            JsonObject rootObj = outData["sensors"].to<JsonObject>();

            for (const auto &[type, devMap] : allData)
            {
                JsonObject typeObj = rootObj[type.c_str()].to<JsonObject>();

                for (const auto &[mac, reading] : devMap)
                {
                    JsonObject devObj = typeObj[mac.toString()].to<JsonObject>();
                    devObj["timestamp"] = reading.timestampMs;
                    devObj["count"] = reading.count;

                    JsonArray vals = devObj["values"].to<JsonArray>();
                    for (uint8_t i = 0; i < reading.count; ++i)
                    {
                        vals.add(reading.values[i]);
                    }
                }
            }

            return {true, "OK"};
        }

        if (action == "calc" || action == "aggregate")
        {
            String type = payload["type"] | payload["args"][1] | "";
            if (type.length() == 0)
            {
                return {false, "Укажите тип датчика (например: sensor calc moisture avg)"};
            }

            String modeStr = payload["mode"] | payload["args"][2] | "avg";
            SensorPickMode mode = parseMode(modeStr);

            int8_t metricIndex = payload["metricIndex"] | payload["args"][3] | -1;

            auto valOpt = aggregator.getAggregatedValue(type.c_str(), mode, metricIndex);
            if (!valOpt.has_value())
            {
                return {false, "Нет актуальных данных для типа '" + type + "'"};
            }

            float result = valOpt.value();
            outData["type"] = type;
            outData["mode"] = modeStr;
            outData["metricIndex"] = metricIndex;
            outData["value"] = result;

            return {true, "Значение для " + type + " (" + modeStr + "): " + String(result, 2)};
        }

        if (action == "cleanup")
        {
            aggregator.cleanupStale();
            return {true, "Устаревшие данные успешно очищены"};
        }

        return {false, "Неизвестное действие. Используйте 'sensor help'"};
    }
};