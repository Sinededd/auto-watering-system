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
#include "controllers/PairingBuffer.h"
#include "controllers/DeviceRegistry.h"


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
PairingBuffer pairingBuffer(eventBus, deviceRegistry);

HostController controller(netMgr, eventBus, pairingBuffer);
DeviceController deviceController(deviceRegistry, pairingBuffer, netMgr, eventBus);

void setup()
{
    Logger::begin();
    LOG_I(TAG, "Start...");

    led.begin();
    valve.begin();
    netMgr.begin();

    webAdapter.begin();
    server.begin();

    controller.begin();
    deviceController.begin();

    cmdRegistry.registerCommand(std::make_unique<HelpCommand>(cmdRegistry));
    cmdRegistry.registerCommand(std::make_unique<LedCommand>(&led));
    cmdRegistry.registerCommand(std::make_unique<ValveCommand>(&valve));
    cmdRegistry.registerCommand(std::make_unique<DeviceCommand>(deviceController, deviceRegistry, pairingBuffer));
 
    eventBus.subscribe(EventType::DEVICE_DISCOVERED, [](const JsonVariantConst& payload) {
        myCli.onDeviceDiscovered(payload);
    });

    eventBus.subscribe(EventType::DEVICE_LOST, [](const JsonVariantConst& payload) {
        myCli.onDeviceLost(payload);
    });

    eventBus.subscribe(EventType::DEVICE_UPDATED, [](const JsonVariantConst& payload) {
        myCli.onDeviceUpdate(payload);
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
    controller.update();
    myCli.update();
    deviceController.update();
    vTaskDelay(pdMS_TO_TICKS(10));
}