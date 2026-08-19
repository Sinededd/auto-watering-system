#pragma once
#include "host/commands/ICommand.h"
#include <Led.h>

class LedCommand : public ICommand {
private:
    Led* led;

public:
    explicit LedCommand(Led* l) : led(l) {}

    String getName() const override { return "led"; }
    String getDescription() const override { return "led <on|off|slow|fast> [count] — Управление LED"; }

    CommandResult execute(const JsonVariantConst& payload, JsonDocument& outData) override {
        if (!led) return {false, "Ошибка: LED не подключен!"};

        String action = payload["action"] | payload["args"][0] | "";
        int count = payload["count"] | (payload["args"][1] ? payload["args"][1].as<String>().toInt() : 0);

        if (action == "on") {
            led->turnOn();
        } else if (action == "off") {
            led->turnOff();
        } else if (action == "slow") {
            led->slowBlinking(count);
        } else if (action == "fast") {
            led->fastBlinking(count);
        } else {
            return {false, "Неизвестный параметр LED: " + action};
        }

        return {true, "Команда LED выполнена: " + action};
    }
};