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

        // Separate the command name and the remaining string of arguments
        int spaceIdx = line.indexOf(' ');
        String cmdName = (spaceIdx == -1) ? line : line.substring(0, spaceIdx);
        String argsStr = (spaceIdx == -1) ? "" : line.substring(spaceIdx + 1);

        JsonDocument doc;
        JsonArray argsArr = doc["args"].to<JsonArray>();

        // Parsing arguments only if they exist
        if (argsStr.length() > 0) {
            int start = 0;
            int end = argsStr.indexOf(' ');
            while (end != -1) {
                argsArr.add(argsStr.substring(start, end));
                start = end + 1;
                end = argsStr.indexOf(' ', start);
            }
            argsArr.add(argsStr.substring(start));
        }

        JsonDocument respData;
        CommandResult res = registry.execute(cmdName, doc.as<JsonVariantConst>(), respData);
        
        Serial.println(res.success ? res.message : "[ОШИБКА] " + res.message);
    }

public:
    explicit CliAdapter(CommandRegistry& reg) : registry(reg) {}

    void onDeviceDiscovered(const JsonVariantConst& payload) {
        Serial.printf("\n[CLI EVENT] Найдено новое устройство: %s (Тип: %d)\n> ", 
                      payload["mac"].as<const char*>(),
                      payload["type"].as<int>());
    }

    void onDeviceLost(const JsonVariantConst& payload) {
        Serial.printf("\n[CLI EVENT] Устройство пропало: %s\n> ", 
                      payload["mac"].as<const char*>());
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