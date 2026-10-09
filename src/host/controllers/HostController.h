#pragma once
#include <EspNowManager.h>
#include <Logger.h>
#include "DeviceController.h"
#include "SensorAggregator.h"

class HostController
{
private:
    static constexpr const char *TAG = "HOST_CONTROLLER";

    DeviceRegistry &deviceRegistry;
    EspNowManager &network;
    EventBus &eventBus;
    SensorAggregator &sensorAggregator;

    void telemetryHandle(const esp_now_recv_info_t *info, const TelemetryPacket &pkt)
    {
        MacAddress srcMac(info->src_addr);
        LOG_D(TAG, "Get telemetry packet from: %s", srcMac.toString().c_str());

        deviceRegistry.updateTelemetry(srcMac, info->rx_ctrl->rssi);

        Device *dev = deviceRegistry.findDevice(srcMac);
        if (dev != nullptr)
        {
            JsonDocument doc = deviceHeaderToJson(dev->header);

            JsonArray valuesArr = doc["values"].to<JsonArray>();
            uint8_t validCount = (pkt.count > 3) ? 3 : pkt.count;
            for (uint8_t i = 0; i < validCount; ++i)
            {
                valuesArr.add(pkt.values[i]);
            }

            eventBus.publish(EventType::DEVICE_TELEMETRY, doc.as<JsonVariantConst>());

            if (dev->header.type[0] != '\0')
                sensorAggregator.addReading(dev->header.type, srcMac, pkt);
        }
    }

public:
    HostController(DeviceRegistry &deviceRegistry, EventBus &eventBus, EspNowManager &network, SensorAggregator &sensorAggregator)
        : deviceRegistry(deviceRegistry), eventBus(eventBus), network(network), sensorAggregator(sensorAggregator) {}

    void begin()
    {
        network.onPacket<TelemetryPacket>(
            PacketType::TELEMETRY_REPORT,
            [this](const esp_now_recv_info_t *info, const TelemetryPacket &pkt)
            {
                this->telemetryHandle(info, pkt);
            });
    }
};
