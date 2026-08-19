#pragma once
#include "host/commands/ICommand.h"
#include "../hardware/Valve.h"

class ValveCommand : public ICommand {
private:
    Valve* valve;

public:
    explicit ValveCommand(Valve* v) : valve(v) {}

    String getName() const override { return "valve"; }
    String getDescription() const override { return "valve [open|close] — Управление клапаном"; }

    CommandResult execute(const JsonVariantConst& payload, JsonDocument& outData) override {
        if (!valve) return {false, "Ошибка: Клапан не подключен!"};

        String action = payload["action"] | payload["args"][0] | "";

        if (action == "") {
            return {true, String("Текущее состояние клапана: ") + (valve->isOpen() ? "opened" : "closed")};
        } else if (action == "open") {
            valve->open();
        } else if (action == "close") {
            valve->close();
        } else {
            return {false, "Неизвестный параметр клапана: " + action};
        }

        return {true, "Команда клапана выполнена: " + action};
    }
};