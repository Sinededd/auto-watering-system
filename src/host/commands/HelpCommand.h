#pragma once
#include "ICommand.h"
#include "CommandRegistry.h"

class HelpCommand : public ICommand {
private:
    const CommandRegistry& registry;

public:
    explicit HelpCommand(const CommandRegistry& reg) : registry(reg) {}

    String getName() const override { 
        return "help"; 
    }

    String getDescription() const override { 
        return "help — отобразить список всех доступных команд"; 
    }

    CommandResult execute(const JsonVariantConst& payload, JsonDocument& outData) override {
        String output = "\n=== Доступные команды ===\n";
        
        JsonArray commandsArr = outData["commands"].to<JsonArray>();

        for (const auto& [name, cmd] : registry.getCommands()) {
            output += " • " + cmd->getDescription() + "\n";

            JsonObject obj = commandsArr.add<JsonObject>();
            obj["name"] = cmd->getName();
            obj["description"] = cmd->getDescription();
        }

        output += "=========================\n";

        return {true, output};
    }
};