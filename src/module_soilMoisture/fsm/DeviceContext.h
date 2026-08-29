#pragma once
#include <memory>
#include "IDeviceState.h"
#include "StorageManager.h"
#include <EspNowManager.h>
#include <Led.h>
#include <module_soilMoisture/controllers/SleepController.h>
#include <module_soilMoisture/controllers/TelemetryController.h>
#include <Button2.h>
#include "WakeupState.h"
#include "module_soilMoisture/AppConfig.h"

struct NetworkConfig
{
    MacAddress hostMac;
    uint8_t channel;
    bool isPaired;
};

class DeviceContext
{
private:
    static constexpr const char *TAG = "DEVICE_CONTEXT";

    EspNowManager &net;
    StorageManager &storage;
    Led &led;
    Button2 &button;
    SleepController &sleepController;
    TelemetryController &telemetryController;

    static RtcCache<NetworkConfig> rtcNetCache;

    std::unique_ptr<IDeviceState> currentState;

    void executeFullReset()
    {
        LOG_I(TAG, "!!!RESET DEVICE TO FACTORY!!!");
        led.fastBlinkingBlocking(5);
        AppConfig::resetToFactory();
        net.randomizeMacAddress();
        storage.remove<NetworkConfig>("net_cfg", &rtcNetCache);
        ESP.restart();
    }

public:
    DeviceContext(EspNowManager &netMgr, StorageManager &storageMgr, Led &led, Button2 &button, SleepController &sleepController, TelemetryController &telemetryController)
        : net(netMgr), storage(storageMgr), led(led), button(button), sleepController(sleepController), telemetryController(telemetryController) {}

    void begin(std::unique_ptr<IDeviceState> initialState)
    {
        button.setLongClickDetectedHandler([this](Button2 &btn) { 
            executeFullReset(); 
        });

        button.setClickHandler([this](Button2 &btn) { 
            transitionTo(std::make_unique<WakeupState>()); 
        });

        transitionTo(std::move(initialState));
    }

    void transitionTo(std::unique_ptr<IDeviceState> newState)
    {
        if (currentState)
        {
            currentState->onExit(*this);
        }
        currentState = std::move(newState);
        if (currentState)
        {

            currentState->onEnter(*this);
        }
    }

    void update()
    {
        button.loop();
        if (currentState)
            currentState->update(*this);
    }

    void handleButton()
    {
        if (currentState)
            currentState->onButtonPressed(*this);
    }

    EspNowManager &getNet() { return net; }
    StorageManager &getStorage() { return storage; }
    Led &getLed() { return led; }
    RtcCache<NetworkConfig>* getRtcNetCache() { return &rtcNetCache; }
    SleepController &getSleepController() { return sleepController; }
    TelemetryController &getTelemetryController() { return telemetryController; }
};