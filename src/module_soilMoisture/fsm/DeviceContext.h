#pragma once
#include <memory>
#include "IDeviceState.h"
#include "StorageManager.h"
#include <EspNowManager.h>
#include <Led.h>
#include <module_soilMoisture/controllers/SleepController.h>

struct NetworkConfig {
    MacAddress hostMac;
    uint8_t channel = 0;
    bool isPaired;
};

class DeviceContext {
private:
    EspNowManager& net;
    StorageManager& storage;
    Led& led;
    SleepController &sleepController;
    static inline RTC_DATA_ATTR RtcCache<NetworkConfig> rtcNetCache;

    std::unique_ptr<IDeviceState> currentState;

public:
    DeviceContext(EspNowManager& netMgr, StorageManager& storageMgr, Led& led, SleepController &sleepController)
        : net(netMgr), storage(storageMgr), led(led), sleepController(sleepController) {}

    void begin(std::unique_ptr<IDeviceState> initialState) {
        transitionTo(std::move(initialState));
    }

    void transitionTo(std::unique_ptr<IDeviceState> newState) {
        if (currentState) {
            currentState->onExit(*this);
        }
        currentState = std::move(newState);
        if (currentState) {

            currentState->onEnter(*this);
        }
    }

    void update() {
        if (currentState) currentState->update(*this);
    }

    void handleButton() {
        if (currentState) currentState->onButtonPressed(*this);
    }

    EspNowManager& getNet() { return net; }
    StorageManager& getStorage() { return storage; }
    Led& getLed() {return led; }
    RtcCache<NetworkConfig>* getRtcNetCache() { return &rtcNetCache; }
    SleepController& getSleepController() { return sleepController; }
};