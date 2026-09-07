#include "Logger.h"
#include "Config.h"
#include <Led.h>
#include <EspNowManager.h>
#include <StorageManager.h>

#include "registry/DeviceRegistry.h"
#include "controllers/DeviceController.h"
#include "controllers/HostController.h"
#include "controllers/IrrigationController.h"
#include "controllers/IrrigationThresholds.h"
#include "controllers/SensorAggregator.h"

#include "commands/CommandRegistry.h"
#include "commands/DeviceCommand.h"
#include "commands/HelpCommand.h"
#include "commands/LedCommand.h"
#include "commands/SensorCommand.h"
#include "commands/ValveCommand.h"

#include "adapters/CliAdapter.h"
#include "adapters/WebAdapter.h"

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

void setup()
{
    Logger::begin();
    LOG_I(TAG, "Start...");

    led.begin();
    valve.begin();
    deviceRegistry.begin();
    netMgr.begin();

    // Загружаем пороги из NVS в оперативную память
    thresholds.begin();

    webAdapter.begin();
    server.begin();

    deviceController.begin();
    hostController.begin();
    irrigationController.begin();
    irrigationController.setTimings(60000, 60000); // 1 мин полив / 1 мин пауза
    
    cmdRegistry.registerCommand(std::make_unique<HelpCommand>(cmdRegistry));
    cmdRegistry.registerCommand(std::make_unique<LedCommand>(&led));
    cmdRegistry.registerCommand(std::make_unique<ValveCommand>(&valve));
    cmdRegistry.registerCommand(std::make_unique<DeviceCommand>(deviceController, deviceRegistry));
    cmdRegistry.registerCommand(std::make_unique<SensorCommand>(sensorAggregator));
 
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
}

void loop()
{
    led.update();
    valve.update();
    deviceController.update();
    myCli.update();
    irrigationController.update();
    
    vTaskDelay(pdMS_TO_TICKS(10));
}