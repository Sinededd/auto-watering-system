#pragma once

#include <Arduino.h>
#include <vector>
#include <optional>
#include <ArduinoJson.h>
#include "Device.h"
#include "Packets.h"
#include "StorageManager.h"

class DeviceRegistry
{
private:
    static constexpr const char *TAG = "DEVICE_REGISTRY";
    static constexpr const char *INDEX_KEY = "dev_index";
    static constexpr size_t MAX_DEVICES = 16;

    StorageManager &storage;
    std::vector<Device> devices;

    String makeNvsKey(const MacAddress &mac) const
    {
        char buf[15];
        const uint8_t *b = mac.bytes;
        snprintf(buf, sizeof(buf), "d_%02X%02X%02X%02X%02X%02X",
                 b[0], b[1], b[2], b[3], b[4], b[5]);
        return String(buf);
    }

    void saveIndex()
    {
        struct MacIndex
        {
            uint8_t count;
            MacAddress macs[MAX_DEVICES];
        } indexObj{};

        indexObj.count = min(devices.size(), MAX_DEVICES);
        for (size_t i = 0; i < indexObj.count; ++i)
        {
            indexObj.macs[i] = devices[i].header.mac;
        }

        storage.save(INDEX_KEY, indexObj);
    }

public:
    explicit DeviceRegistry(StorageManager &mgr) : storage(mgr) {}

    /// @brief Loading all saved devices from NVS to RAM
    void begin()
    {
        devices.clear();

        struct MacIndex
        {
            uint8_t count;
            MacAddress macs[MAX_DEVICES];
        };

        auto indexOpt = storage.load<MacIndex>(INDEX_KEY);
        if (!indexOpt.has_value())
        {
            LOG_I(TAG, "No device index found in NVS");
            return;
        }

        const auto &index = indexOpt.value();
        for (uint8_t i = 0; i < index.count; ++i)
        {
            String key = makeNvsKey(index.macs[i]);
            auto devOpt = storage.load<Device>(key.c_str());
            if (devOpt.has_value())
            {
                devices.push_back(devOpt.value());
                Device &loaded = devices.back();
                if (loaded.header.type[0] == '\0' && loaded.interface.metricCount > 0 &&
                    loaded.interface.metrics[0].label[0] != '\0')
                {
                    strlcpy(loaded.header.type, loaded.interface.metrics[0].label,
                            sizeof(loaded.header.type));
                    saveDeviceToNvs(loaded);
                    LOG_I(TAG, "Restored type '%s' for device %s",
                          loaded.header.type, loaded.header.mac.toArduinoString().c_str());
                }
                LOG_I(TAG, "Loaded device %s from NVS", index.macs[i].toArduinoString().c_str());
            }
        }
    }

    /// @brief Find device by MAC
    Device *findDevice(const MacAddress &mac)
    {
        for (auto &dev : devices)
        {
            if (dev.header.mac == mac)
            {
                return &dev;
            }
        }
        LOG_W(TAG, "Not found device %s", mac.toString().c_str());
        return nullptr;
    }

    /// @brief Process SettingsReportPacket (Create or update existing)
    void processSettingsReport(const MacAddress &mac, int8_t rssi, const SettingsReportPacket &pkt, const char *devType = nullptr)
    {
        Device *dev = findDevice(mac);
        bool isNew = (dev == nullptr);

        if (isNew)
        {
            if (devices.size() >= MAX_DEVICES)
            {
                LOG_E(TAG, "Device limit reached! Cannot add %s", mac.toArduinoString().c_str());
                return;
            }
            Device newDev{};
            newDev.header.mac = mac;
            newDev.header.status = DeviceStatus::CONNECTING;

            if (devType && devType[0] != '\0')
            {
                strlcpy(newDev.header.type, devType, sizeof(newDev.header.type));
            }

            devices.push_back(newDev);
            dev = &devices.back();
        }
        else if (devType && devType[0] != '\0' && dev->header.type[0] == '\0')
        {
            strlcpy(dev->header.type, devType, sizeof(dev->header.type));
        }

        dev->header.sigLevel = rssi;
        dev->header.lastSeenMs = millis();
        dev->settings = pkt.settings;
        dev->interface = pkt.interface;

        if (isNew)
        {
            saveDeviceToNvs(*dev);
            saveIndex();
            LOG_I(TAG, "New device %s (%s) registered & saved to NVS",
                  mac.toArduinoString().c_str(), dev->header.type);
        }
        else
        {
            LOG_D(TAG, "Device %s settings updated in RAM", mac.toArduinoString().c_str());
        }
    }

    /// @brief Fast telemetry update when receiving any package (RAM only)
    void updateTelemetry(const MacAddress &mac, int8_t rssi)
    {
        Device *dev = findDevice(mac);
        if (dev)
        {
            dev->header.sigLevel = rssi;
            dev->header.lastSeenMs = millis();
        }
    }

    /// @brief Change the device status (saved to NVS)
    bool setDeviceStatus(const MacAddress &mac, DeviceStatus status)
    {
        Device *dev = findDevice(mac);
        if (!dev)
            return false;

        if (dev->header.status != status)
        {
            dev->header.status = status;
            saveDeviceToNvs(*dev);
        }
        return true;
    }

    /// @brief Force save device structure in NVS
    bool saveDeviceToNvs(const Device &dev)
    {
        String key = makeNvsKey(dev.header.mac);
        return storage.save(key.c_str(), dev);
    }

    /// @brief Remove device from RAM and NVS
    bool removeDevice(const MacAddress &mac)
    {
        for (auto it = devices.begin(); it != devices.end(); ++it)
        {
            if (it->header.mac == mac)
            {
                String key = makeNvsKey(mac);
                storage.remove(key.c_str());
                devices.erase(it);
                saveIndex();
                LOG_I(TAG, "Device %s removed", mac.toArduinoString().c_str());
                return true;
            }
        }
        return false;
    }

    /// @brief Get all devices to serialize into JSON
    const std::vector<Device> &getAllDevices() const
    {
        return devices;
    }

    /// @brief Generate a full JSON list of devices for Web/API
    void toJson(JsonArray array) const
    {
        for (const auto &dev : devices)
        {
            JsonObject obj = array.add<JsonObject>();
            deviceToJson(dev, obj);
        }
    }
};
