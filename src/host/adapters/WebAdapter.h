#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "host/commands/CommandRegistry.h"
#include "host/events/EventBus.h"

class WebAdapter
{
private:
    AsyncWebServer &server;
    CommandRegistry &registry;
    AsyncEventSource events;

    // Processing incoming POST /api/command
    void handleCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index)
    {
        if (index != 0)
            return;

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
    }

    void sendSseEvent(const char *eventName, const JsonVariantConst &payload)
    {
        JsonDocument doc;
        doc["payload"] = payload;

        String jsonString;
        serializeJson(doc, jsonString);

        events.send(jsonString.c_str(), eventName, millis());
    }

public:
    WebAdapter(AsyncWebServer &webServer, CommandRegistry &cmdRegistry)
        : server(webServer), registry(cmdRegistry), events("/api/events") {}

    void begin()
    {
        server.addHandler(&events);

        server.on(
            "/api/command",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {},
            nullptr,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
                this->handleCommand(request, data, len, index);
            });
    }

    void onDeviceDiscovered(const JsonVariantConst &payload)
    {
        sendSseEvent("device_discovered", payload);
    }

    void onDeviceLost(const JsonVariantConst &payload)
    {
        sendSseEvent("device_lost", payload);
    }
};