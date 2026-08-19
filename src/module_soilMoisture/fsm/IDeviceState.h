#pragma once
#include <Arduino.h>
#include <esp_now.h>

class DeviceContext;

class IDeviceState
{  
protected:
    static constexpr const char *TAG = "State";

public:
    virtual ~IDeviceState() = default;

    virtual void onEnter(DeviceContext &ctx) {}
    virtual void onExit(DeviceContext &ctx) {}

    virtual void update(DeviceContext &ctx) {}

    virtual void onButtonPressed(DeviceContext &ctx) {}
};