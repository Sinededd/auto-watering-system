#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "host/commands/CommandRegistry.h"


class CliAdapter {
private:
    CommandRegistry& registry;
    char buffer[64];
    uint8_t pos = 0;

    void processLine(const String& line) {
    if (line.length() == 0) return;

    int spaceIdx = line.indexOf(' ');
    String cmdName = (spaceIdx == -1) ? line : line.substring(0, spaceIdx);
    String argsStr = (spaceIdx == -1) ? "" : line.substring(spaceIdx + 1);

    JsonDocument doc;
    JsonArray argsArr = doc["args"].to<JsonArray>();

    if (argsStr.length() > 0) {
        int start = 0;
        int end = argsStr.indexOf(' ');
        while (end != -1) {
            String arg = argsStr.substring(start, end);
            if (arg.length() > 0) argsArr.add(arg);
            start = end + 1;
            end = argsStr.indexOf(' ', start);
        }
        String lastArg = argsStr.substring(start);
        if (lastArg.length() > 0) argsArr.add(lastArg);
    }

    JsonDocument respData;
    CommandResult res = registry.execute(cmdName, doc.as<JsonVariantConst>(), respData);

    Serial.println(res.success ? res.message : "[ОШИБКА] " + res.message);

    if (!respData.isNull() && respData.size() > 0) {
        serializeJsonPretty(respData, Serial);
        Serial.println();
    }
}

public:
    explicit CliAdapter(CommandRegistry& reg) : registry(reg) {}

    void onDeviceDiscovered(const JsonVariantConst& payload) {
        Serial.printf("\n[CLI EVENT] Найдено новое устройство: %s (Тип: %s)\n", 
                      payload["mac"].as<const char*>(),
                      payload["type"].as<const char*>());
    }

    void onDeviceLost(const JsonVariantConst& payload) {
        Serial.printf("\n[CLI EVENT] Устройство пропало: %s\n", 
                      payload["mac"].as<const char*>());
    }

    void onDeviceUpdate(const JsonVariantConst& payload) {
        Serial.printf("\n[CLI EVENT] Изменился статус устройства: %s (Статус: %d)\n", 
                    payload["mac"].as<const char*>(),
                    payload["status"].as<uint8_t>());
    }

    void onDeviceTelemetry(const JsonVariantConst& payload) 
    {
        const char* mac = payload["mac"] | "00:00:00:00:00:00";
        uint8_t status = payload["status"] | 0;
        const char* statusStr = payload["statusStr"] | "UNKNOWN";
        int rssi = payload["sigLevel"] | 0;
        uint32_t lastSeenSec = payload["lastSeenSecAgo"] | 0;

        Serial.printf("\n[CLI EVENT] Устройство: %s | Статус: %s (%u) | RSSI: %d dBm | Был в сети: %u с. назад\n", 
                    mac, statusStr, status, rssi, lastSeenSec);

        JsonArrayConst values = payload["values"].as<JsonArrayConst>();
        if (!values.isNull() && values.size() > 0) 
        {
            Serial.print("  [Данные]: ");
            for (size_t i = 0; i < values.size(); ++i) 
            {
                Serial.printf("val[%d]: %.2f%s", 
                            i, 
                            values[i].as<float>(), 
                            (i < values.size() - 1) ? " | " : "\n");
            }
        }
    }

    void update() {
        while (Serial.available() > 0) {
            char c = Serial.read();

            if (c == '\r' || c == '\n') {
                if (pos > 0) {
                    buffer[pos] = '\0';
                    processLine(String(buffer));
                    pos = 0;
                }
            } else if (pos < sizeof(buffer) - 1) {
                buffer[pos++] = c;
            }
        }
    }
};