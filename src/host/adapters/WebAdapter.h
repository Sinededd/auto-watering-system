#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "host/commands/CommandRegistry.h"

/// @brief Configures the `/api/command` HTTP POST endpoint to execute JSON commands (max payload size: 1024 bytes).
void setupWebAdapter(AsyncWebServer &server, CommandRegistry &registry)
{
    server.on(
        "/api/command",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        [&registry](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (index != 0) return;

            JsonDocument reqDoc;
            if (deserializeJson(reqDoc, data, len))
            {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *cmdName = reqDoc["cmd"];
            if (!cmdName)
            {
                request->send(400, "application/json", "{\"error\":\"Missing 'cmd'\"}");
                return;
            }

            JsonDocument respData;
            CommandResult result = registry.execute(String(cmdName), reqDoc["payload"].as<JsonVariantConst>(), respData);

            JsonDocument finalResp;
            finalResp["success"] = result.success;
            finalResp["message"] = result.message;
            if (!respData.isNull())
            {
                finalResp["data"] = respData;
            }

            String response;
            serializeJson(finalResp, response);
            request->send(200, "application/json", response);
        });
}