#pragma once
#include <Arduino.h>
#include <StorageManager.h>
#include <host/models/Device.h>
#include <optional>

class DeviceRegistry
{
private:
    static constexpr const char *TAG = "DeviceRegistry";
    static constexpr const char *STORAGE_KEY = "dev_list";
    using ListType = DeviceListStorage<16>;

    StorageManager &storage;
    ListType cache;
    bool isLoaded = false;

    void ensureLoaded()
    {
        if (!isLoaded)
        {
            cache = storage.load<ListType>(STORAGE_KEY).value_or(ListType{});
            isLoaded = true;
        }
    }

public:
    explicit DeviceRegistry(StorageManager &storageMgr) : storage(storageMgr) {}

    void begin()
    {
        ensureLoaded();
    }

    void onPairRequest(const MacAddress &mac, DeviceType type)
    {
        if (registry.getByMac(mac).has_value())
        {
            return;
        }

        uint32_t now = millis();

        for (auto &item : queue)
        {
            if (item.device.header.mac == mac)
            {
                item.lastSeenMs = now;
                return;
            }
        }

        DiscoveredDevice item;
        item.device.header.mac = mac;
        item.device.header.type = type;
        item.device.header.status = DeviceStatus::UNPAIRED;
        item.device.settings.isValid = false;
        item.lastSeenMs = now;

        queue.push_back(item);
        LOG_I(TAG, "New device found nearby: %s (%s)",
              item.device.header.mac.toString().c_str(),
              deviceTypeToRussian(item.device.header.type).c_str());

        JsonDocument doc = deviceToJson(item.device);
        eventBus.publish(EventType::DEVICE_DISCOVERED, doc.as<JsonVariantConst>());
    }

    bool save(const Device &device)
    {
        ensureLoaded();

        int foundIndex = -1;
        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (cache.items[i].header.mac == device.header.mac)
            {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex >= 0)
        {
            cache.items[foundIndex] = device;
        }
        else
        {
            if (cache.count >= 32)
            {
                return false;
            }
            cache.items[cache.count] = device;
            cache.count++;
        }

        return storage.save(STORAGE_KEY, cache);
    }

    std::optional<Device> getByMac(const MacAddress &mac)
    {
        ensureLoaded();

        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (cache.items[i].header.mac == mac)
            {
                return cache.items[i];
            }
        }

        return std::nullopt;
    }

    bool remove(const MacAddress &mac)
    {
        ensureLoaded();

        if (cache.count == 0)
        {
            return false;
        }

        int foundIndex = -1;
        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (cache.items[i].header.mac == mac)
            {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex == -1)
        {
            return false;
        }

        for (uint8_t i = foundIndex; i < cache.count - 1; ++i)
        {
            cache.items[i] = cache.items[i + 1];
        }

        cache.count--;

        return storage.save(STORAGE_KEY, cache);
    }

    std::optional<ListType> getAll()
    {
        ensureLoaded();
        return cache;
    }
};