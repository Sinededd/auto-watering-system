#pragma once
#include <memory>
#include "IDeviceState.h"
#include "StorageManager.h"
#include <EspNowManager.h>
#include <Led.h>

struct NetworkConfig {
    uint8_t hostMac[6];
    uint8_t channel = 0;
    bool isPaired;
};

class DeviceContext {
private:
    EspNowManager& net;
    StorageManager& storage;
    Led& led;
    static inline RTC_DATA_ATTR RtcCache<NetworkConfig> rtcNetCache;

    std::unique_ptr<IDeviceState> currentState;

public:
    DeviceContext(EspNowManager& netMgr, StorageManager& storageMgr, Led& led)
        : net(netMgr), storage(storageMgr), led(led) {}

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
};