#pragma once
#include <map>
#include <memory>
#include <Logger.h>
#include "ICommand.h"

class CommandRegistry
{
private:
    std::map<String, std::unique_ptr<ICommand>> commands;

public:
    void registerCommand(std::unique_ptr<ICommand> cmd)
    {
        String name = cmd->getName();
        commands[name] = std::move(cmd);
        LOG_I("CMD", "Registered: %s", name.c_str());
    }

    CommandResult execute(const String &name, const JsonVariantConst &payload, JsonDocument &outData)
    {
        auto it = commands.find(name);
        if (it == commands.end())
        {
            return {false, "Неизвестная команда: '" + name + "'"};
        }
        return it->second->execute(payload, outData);
    }

    const auto &getCommands() const { return commands; }
};