#pragma once
#include <ArduinoJson.h>
#include "ICommand.h"
#include "host/controllers/IrrigationController.h"

class IrrigationCommand : public ICommand
{
private:
    IrrigationController &irrigationController;

    String helpCommand()
    {
        return "Использование:\n"
               "  irrigation status — текущее состояние и настройки\n"
               "  irrigation enable|on — включить автополив\n"
               "  irrigation disable|off — выключить автополив\n"
               "  irrigation start — принудительно запустить полив\n"
               "  irrigation stop — принудительно остановить полив\n"
               "  irrigation timing <duration_sec> <cooldown_sec> — задать длительность и паузу (в секундах)";
    }

    String stateToString(IrrigationState state)
    {
        switch (state)
        {
        case IrrigationState::OFF: return "ВЫКЛЮЧЕН (OFF)";
        case IrrigationState::IDLE: return "ОЖИДАНИЕ (IDLE)";
        case IrrigationState::WATERING: return "ПОЛИВ (WATERING)";
        case IrrigationState::COOLDOWN: return "ПАУЗА (COOLDOWN)";
        case IrrigationState::ERROR_NO_DATA: return "ОШИБКА: НЕТ ДАННЫХ";
        default: return "НЕИЗВЕСТНО";
        }
    }

public:
    explicit IrrigationCommand(IrrigationController &irrigationController)
        : irrigationController(irrigationController) {}

    String getName() const override { return "irrigation"; }
    String getDescription() const override { return "irrigation [help|status|enable|disable|start|stop|timing] — Управление контроллером полива"; }

    CommandResult execute(const JsonVariantConst &payload, JsonDocument &outData) override
    {
        String action = payload["action"] | payload["args"][0] | "";

        if (action == "help" || action == "")
        {
            return {true, helpCommand()};
        }

        // --- СТАТУС ---
        if (action == "status" || action == "info")
        {
            const auto &config = irrigationController.getConfig();
            IrrigationState state = irrigationController.getState();
            uint32_t remCooldownSec = irrigationController.getRemainingCooldownMs() / 1000;

            outData["enabled"] = config.enabled;
            outData["state"] = stateToString(state);
            outData["duration_sec"] = config.wateringDurationMs / 1000;
            outData["cooldown_sec"] = config.cooldownDurationMs / 1000;
            outData["remaining_cooldown_sec"] = remCooldownSec;

            String msg = "Статус автополива:\n";
            msg += " - Состояние: " + stateToString(state) + "\n";
            msg += " - Автополив: " + String(config.enabled ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") + "\n";
            msg += " - Длительность полива: " + String(config.wateringDurationMs / 1000) + " сек\n";
            msg += " - Пауза между поливами: " + String(config.cooldownDurationMs / 1000) + " сек\n";
            if (state == IrrigationState::COOLDOWN)
            {
                msg += " - Осталось паузы: " + String(remCooldownSec) + " сек\n";
            }

            return {true, msg};
        }

        // --- ВКЛЮЧЕНИЕ ---
        if (action == "enable" || action == "on")
        {
            if (irrigationController.setEnabled(true))
            {
                return {true, "Автополив успешно ВКЛЮЧЕН"};
            }
            return {false, "Ошибка при сохранении состояния в NVS"};
        }

        // --- ВЫКЛЮЧЕНИЕ ---
        if (action == "disable" || action == "off")
        {
            if (irrigationController.setEnabled(false))
            {
                return {true, "Автополив успешно ВЫКЛЮЧЕН (клапан закрыт)"};
            }
            return {false, "Ошибка при сохранении состояния в NVS"};
        }

        // --- ПРИНУДИТЕЛЬНЫЙ СТАРТ ---
        if (action == "start" || action == "water")
        {
            irrigationController.forceStart();
            return {true, "Принудительный полив запущен"};
        }

        // --- ПРИНУДИТЕЛЬНАЯ ОСТАНОВКА ---
        if (action == "stop")
        {
            irrigationController.forceStop();
            return {true, "Полив остановлен (переход в режим cooldown)"};
        }

        // --- НАСТРОЙКА ТАЙМИНГОВ ---
        if (action == "timing" || action == "timings" || action == "set_timing")
        {
            String durStr = payload["duration"] | payload["args"][1] | "";
            String cdStr = payload["cooldown"] | payload["args"][2] | "";

            if (durStr.length() == 0 || cdStr.length() == 0)
            {
                return {false, "Укажите длительность полива и паузу в секундах (например: irrigation timing 60 600)"};
            }

            uint32_t durSec = durStr.toInt();
            uint32_t cdSec = cdStr.toInt();

            if (durSec == 0 || cdSec == 0)
            {
                return {false, "Значения должны быть больше 0"};
            }

            if (irrigationController.setTimings(durSec * 1000, cdSec * 1000))
            {
                return {true, "Тайминги обновлены: полив " + String(durSec) + " сек, пауза " + String(cdSec) + " сек"};
            }
            return {false, "Ошибка при сохранении таймингов в NVS"};
        }

        return {false, "Неизвестное действие. Используйте 'irrigation help'"};
    }
};