#pragma once

#include <Arduino.h>
#include <optional>
#include <vector>
#include <string>
#include <Logger.h>
#include <StorageManager.h>
#include <host/events/EventBus.h>
#include "IrrigationThresholds.h"
#include "host/hardware/Valve.h"
#include "SensorAggregator.h"

struct IrrigationConfig
{
    uint32_t wateringDurationMs = 60000;
    uint32_t cooldownDurationMs = 600000;
    bool enabled = true;
};

enum class IrrigationState
{
    OFF,
    IDLE,
    WATERING,
    COOLDOWN,
    ERROR_NO_DATA
};

class IrrigationController
{
private:
    static constexpr const char *TAG = "IRRIGATION_CTRL";
    static constexpr const char *CONFIG_NVS_KEY = "ctrl_config";

    StorageManager storage; 
    IrrigationThresholds &thresholds;
    SensorAggregator &aggregator;
    Valve &valve;
    EventBus &eventBus;

    IrrigationConfig config;
    IrrigationState currentState = IrrigationState::IDLE;

    uint32_t lastCheckMs = 0;
    uint32_t checkIntervalMs = 2000; 

    uint32_t wateringStartedMs = 0;  
    uint32_t lastWateringEndedMs = 0;

    void evaluate()
    {
        uint32_t now = millis();

        // 1. Автополив выключен
        if (!config.enabled)
        {
            if (valve.isOpen())
            {
                LOG_W(TAG, "Controller disabled! Closing valve.");
                valve.close();
            }
            currentState = IrrigationState::OFF;
            return;
        }

        // 2. ДИНАМИЧЕСКИЙ ОПРОС: Получаем все типы датчиков, присутствующие в памяти
        auto activeSensorData = aggregator.getAllSensorData();

        bool anyNeedsWatering = false;
        bool allTargetsReached = true;
        bool hasAnyMonitoredData = false;

        for (const auto &[sensorType, devMap] : activeSensorData)
        {
            // Проверяем, настроены ли пороги для данного типа датчика
            auto threshOpt = thresholds.getThresholds(sensorType);
            if (!threshOpt.has_value())
            {
                // Для этого типа нет порогов полива (например, технический датчик) -> пропускаем
                continue;
            }

            const Thresholds &thresh = threshOpt.value();

            // Запрашиваем актуальное агрегированное значение
            auto valOpt = aggregator.getAggregatedValue(sensorType, thresh.mode);
            if (!valOpt.has_value())
            {
                continue;
            }

            hasAnyMonitoredData = true;
            float currentVal = valOpt.value();

            // Если хотя бы один датчик опустился ниже minThreshold
            if (currentVal < thresh.minThreshold)
            {
                anyNeedsWatering = true;
            }

            // Пока хотя бы один датчик не достиг maxThreshold — цель не считается полностью достигнутой
            if (currentVal < thresh.maxThreshold)
            {
                allTargetsReached = false;
            }
        }

        // 3. Защита: Если ни по одному из отслеживаемых датчиков нет свежих данных
        if (!hasAnyMonitoredData)
        {
            if (valve.isOpen())
            {
                stopWatering("no_sensor_data");
            }
            currentState = IrrigationState::ERROR_NO_DATA;
            return;
        }

        // 4. Логика FSM
        switch (currentState)
        {
        case IrrigationState::WATERING:
        {
            uint32_t elapsedWatering = now - wateringStartedMs;

            if (allTargetsReached)
            {
                LOG_I(TAG, "All target thresholds reached max level. Stopping watering.");
                stopWatering("target_reached");
            }
            else if (elapsedWatering >= config.wateringDurationMs)
            {
                LOG_I(TAG, "Watering duration limit reached (%u ms). Stopping.", (unsigned)config.wateringDurationMs);
                stopWatering("duration_limit");
            }
            break;
        }

        case IrrigationState::COOLDOWN:
        {
            uint32_t elapsedCooldown = now - lastWateringEndedMs;

            if (elapsedCooldown >= config.cooldownDurationMs)
            {
                LOG_I(TAG, "Cooldown finished (%u ms elapsed). Returning to IDLE.", (unsigned)elapsedCooldown);
                currentState = IrrigationState::IDLE;
            }
            break;
        }

        case IrrigationState::IDLE:
        case IrrigationState::ERROR_NO_DATA:
        case IrrigationState::OFF:
        {
            if (lastWateringEndedMs > 0 && (now - lastWateringEndedMs < config.cooldownDurationMs))
            {
                currentState = IrrigationState::COOLDOWN;
                break;
            }

            if (anyNeedsWatering)
            {
                LOG_I(TAG, "Min threshold breached on monitored sensor(s). Starting watering.");
                startWatering();
            }
            else
            {
                currentState = IrrigationState::IDLE;
            }
            break;
        }
        }
    }

    void startWatering()
    {
        valve.open();
        wateringStartedMs = millis();
        currentState = IrrigationState::WATERING;

        publishEvent("watering_started");
    }

    void stopWatering(const char *reason)
    {
        valve.close();
        lastWateringEndedMs = millis();
        currentState = IrrigationState::COOLDOWN;

        publishEvent("watering_stopped", reason);
    }

    void publishEvent(const char *action, const char *reason = nullptr)
    {
        JsonDocument doc;
        doc["action"] = action;
        if (reason) doc["reason"] = reason;
        doc["state"] = static_cast<int>(currentState);
        
        eventBus.publish(EventType::IRRIGATION_STATE_CHANGED, doc.as<JsonVariantConst>());
    }

public:
    // Упрощенный конструктор без указания конкретных типов
    IrrigationController(
        IrrigationThresholds &thresholds,
        SensorAggregator &aggregator,
        Valve &valve,
        EventBus &eventBus
    ) : storage("irr_ctrl"),
        thresholds(thresholds),
        aggregator(aggregator),
        valve(valve),
        eventBus(eventBus) {}

    void begin()
    {
        valve.begin();

        auto loadedCfg = storage.load<IrrigationConfig>(CONFIG_NVS_KEY);
        if (loadedCfg.has_value())
        {
            config = loadedCfg.value();
            LOG_I(TAG, "Config loaded from NVS: duration=%u ms, cooldown=%u ms, enabled=%d",
                  (unsigned)config.wateringDurationMs, (unsigned)config.cooldownDurationMs, config.enabled);
        }
        else
        {
            LOG_I(TAG, "Using default config. Saving to NVS...");
            storage.save(CONFIG_NVS_KEY, config);
        }

        currentState = config.enabled ? IrrigationState::IDLE : IrrigationState::OFF;
        LOG_I(TAG, "Irrigation Controller initialized with dynamic sensor discovery");
    }

    void update()
    {
        valve.update();

        uint32_t now = millis();
        if (now - lastCheckMs >= checkIntervalMs)
        {
            lastCheckMs = now;
            aggregator.cleanupStale();
            evaluate();
        }
    }

    bool setTimings(uint32_t durationMs, uint32_t cooldownMs)
    {
        config.wateringDurationMs = durationMs;
        config.cooldownDurationMs = cooldownMs;
        LOG_I(TAG, "Updated timings: duration=%u ms, cooldown=%u ms", (unsigned)durationMs, (unsigned)cooldownMs);
        return storage.save(CONFIG_NVS_KEY, config);
    }

    bool setEnabled(bool enable)
    {
        config.enabled = enable;
        if (!enable)
        {
            if (valve.isOpen())
            {
                valve.close();
                lastWateringEndedMs = millis();
                publishEvent("watering_stopped", "disabled_by_user");
            }
            currentState = IrrigationState::OFF;
        }
        else if (currentState == IrrigationState::OFF)
        {
            currentState = IrrigationState::IDLE;
        }

        LOG_I(TAG, "Controller %s", config.enabled ? "ENABLED" : "DISABLED");
        return storage.save(CONFIG_NVS_KEY, config);
    }

    void forceStart()
    {
        LOG_I(TAG, "Manual watering force started");
        if (!config.enabled)
        {
            config.enabled = true;
            storage.save(CONFIG_NVS_KEY, config);
        }
        startWatering();
    }

    void forceStop()
    {
        LOG_I(TAG, "Manual watering force stopped");
        stopWatering("manual_stop");
    }

    const IrrigationConfig &getConfig() const { return config; }
    IrrigationState getState() const { return currentState; }
    
    uint32_t getRemainingCooldownMs() const
    {
        if (currentState != IrrigationState::COOLDOWN) return 0;
        uint32_t elapsed = millis() - lastWateringEndedMs;
        return (elapsed >= config.cooldownDurationMs) ? 0 : (config.cooldownDurationMs - elapsed);
    }
};