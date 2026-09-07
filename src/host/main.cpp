#include "Logger.h"
#include "Config.h"
#include <Led.h>
#include <EspNowManager.h>
#include <StorageManager.h>
#include "commands/CommandRegistry.h"
#include "adapters/CliAdapter.h"
#include "commands/LedCommand.h"
#include "commands/ValveCommand.h"
#include "commands/HelpCommand.h"
#include "commands/DeviceCommand.h"
#include "controllers/HostController.h"
#include "adapters/WebAdapter.h"
#include "registry/DeviceRegistry.h"
#include "controllers/IrrigationController.h"
#include "commands/SensorCommand.h"


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

DeviceController deviceController(deviceRegistry, netMgr, eventBus);
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

    webAdapter.begin();
    server.begin();

    Thresholds moistureThresh;
    moistureThresh.minThreshold = 30.0f;
    moistureThresh.maxThreshold = 70.0f;
    moistureThresh.mode = SensorPickMode::AVERAGE;
    

    deviceController.begin();
    hostController.begin();
    irrigationController.begin();
    irrigationController.setTimings(60000, 60000); // 1мин / 1 мин
    
    cmdRegistry.registerCommand(std::make_unique<HelpCommand>(cmdRegistry));
    cmdRegistry.registerCommand(std::make_unique<LedCommand>(&led));
    cmdRegistry.registerCommand(std::make_unique<ValveCommand>(&valve));
    cmdRegistry.registerCommand(std::make_unique<DeviceCommand>(deviceController, deviceRegistry));
    cmdRegistry.registerCommand(std::make_unique<SensorCommand>(sensorAggregator));
 
    eventBus.subscribe(EventType::DEVICE_DISCOVERED, [](const JsonVariantConst& payload) {
        myCli.onDeviceDiscovered(payload);
    });

    eventBus.subscribe(EventType::DEVICE_LOST, [](const JsonVariantConst& payload) {
        myCli.onDeviceLost(payload);
    });

    eventBus.subscribe(EventType::DEVICE_UPDATED, [](const JsonVariantConst& payload) {
        myCli.onDeviceUpdate(payload);
    });

    eventBus.subscribe(EventType::DEVICE_TELEMETRY, [](const JsonVariantConst& payload) {
        myCli.onDeviceTelemetry(payload);
    });

    eventBus.subscribe(EventType::DEVICE_DISCOVERED, [](const JsonVariantConst& payload) {
        webAdapter.onDeviceDiscovered(payload);
    });

    eventBus.subscribe(EventType::DEVICE_LOST, [](const JsonVariantConst& payload) {
        webAdapter.onDeviceLost(payload);
    });

}

void loop()
{
    led.update();
    valve.update();
    deviceController.update();
    myCli.update();
    deviceController.update();
    irrigationController.update();
    vTaskDelay(pdMS_TO_TICKS(10));
}