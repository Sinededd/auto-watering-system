#include "Logger.h"
#include "Config.h"
#include <Led.h>
#include <EspNowManager.h>
#include <StorageManager.h>
#include <WiFi.h>

#include "registry/DeviceRegistry.h"
#include "controllers/DeviceController.h"
#include "controllers/HostController.h"
#include "controllers/IrrigationController.h"
#include "controllers/IrrigationThresholds.h"
#include "controllers/SensorAggregator.h"
#include "controllers/CalibrationController.h"

#include "commands/CommandRegistry.h"
#include "commands/DeviceCommand.h"
#include "commands/HelpCommand.h"
#include "commands/LedCommand.h"
#include "commands/SensorCommand.h"
#include "commands/ValveCommand.h"

#include "adapters/CliAdapter.h"
#include "adapters/WebAdapter.h"
#include "commands/ThresholdCommand.h"
#include "commands/IrrigationCommand.h"
#include "commands/CalibrationCommand.h"

static const char *TAG = "MAIN";

Led led(Config::Pins::LED_PIN, false);
Valve valve(Config::Pins::VALVE_PIN);
EspNowManager netMgr(WIFI_MODE_AP);

CommandRegistry cmdRegistry;
AsyncWebServer server(80);
WebAdapter webAdapter(server, cmdRegistry);
CliAdapter myCli(cmdRegistry);

EventBus eventBus;

StorageManager storage;
DeviceRegistry deviceRegistry(storage);

SensorAggregator sensorAggregator(deviceRegistry);
IrrigationThresholds thresholds;

DeviceController deviceController(deviceRegistry, netMgr, eventBus, thresholds);
HostController hostController(deviceRegistry, eventBus, netMgr, sensorAggregator);
IrrigationController irrigationController(thresholds, sensorAggregator, valve, eventBus);
CalibrationController calibrationController(deviceRegistry, netMgr, eventBus);

void setup()
{
    Logger::begin();
    LOG_I(TAG, "Start...");

    led.begin();
    valve.begin();
    deviceRegistry.begin();

    WiFi.mode(WIFI_MODE_AP);
    if (!WiFi.softAP(
            Config::Network::AP_SSID,
            Config::Network::AP_PASSWORD,
            Config::Network::AP_CHANNEL))
    {
        LOG_E(TAG, "Failed to start Wi-Fi access point");
    }
    else
    {
        LOG_I(TAG, "Wi-Fi AP started: %s, IP: %s",
              Config::Network::AP_SSID,
              WiFi.softAPIP().toString().c_str());
    }

    netMgr.begin();

    // Загружаем пороги из NVS в оперативную память
    thresholds.begin();

    webAdapter.begin();
    server.begin();

    deviceController.begin();
    hostController.begin();
    irrigationController.begin();
    calibrationController.begin();
    irrigationController.setTimings(60000, 60000); // 1 мин полив / 1 мин пауза
    
    cmdRegistry.registerCommand(std::make_unique<HelpCommand>(cmdRegistry));
    cmdRegistry.registerCommand(std::make_unique<LedCommand>(&led));
    cmdRegistry.registerCommand(std::make_unique<ValveCommand>(&valve));
    cmdRegistry.registerCommand(std::make_unique<DeviceCommand>(deviceController, deviceRegistry));
    cmdRegistry.registerCommand(std::make_unique<SensorCommand>(sensorAggregator));
    cmdRegistry.registerCommand(std::make_unique<ThresholdCommand>(thresholds));
    cmdRegistry.registerCommand(std::make_unique<IrrigationCommand>(irrigationController));
    cmdRegistry.registerCommand(std::make_unique<CalibrationCommand>(calibrationController));
 
    eventBus.subscribe(EventType::DEVICE_DISCOVERED, [](const JsonVariantConst& payload) {
        myCli.onDeviceDiscovered(payload);
        webAdapter.onDeviceDiscovered(payload);
    });

    eventBus.subscribe(EventType::DEVICE_LOST, [](const JsonVariantConst& payload) {
        myCli.onDeviceLost(payload);
        webAdapter.onDeviceLost(payload);
    });

    eventBus.subscribe(EventType::DEVICE_UPDATED, [](const JsonVariantConst& payload) {
        myCli.onDeviceUpdate(payload);
    });

    eventBus.subscribe(EventType::DEVICE_TELEMETRY, [](const JsonVariantConst& payload) {
        myCli.onDeviceTelemetry(payload);
    });

    eventBus.subscribe(EventType::CALIBRATION_SAMPLE, [](const JsonVariantConst& payload) {
        webAdapter.onCalibrationSample(payload);
    });

    // main events

    eventBus.subscribe(EventType::IRRIGATION_STATE_CHANGED, [](const JsonVariantConst& payload) {
        const char* action = payload["action"] | "unknown";
        const char* reason = payload["reason"] | "";
        LOG_I("MAIN", "Irrigation event: %s %s", action, reason);
    });
}

void loop()
{
    led.update();
    valve.update();
    deviceController.update();
    myCli.update();
    irrigationController.update();
    calibrationController.update();
    
    vTaskDelay(pdMS_TO_TICKS(10));
}
