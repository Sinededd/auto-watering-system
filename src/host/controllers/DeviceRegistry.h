#pragma once
#include <Arduino.h>
#include <StorageManager.h>
#include <host/models/Device.h>
#include <Converter.h>
#include <optional>

class DeviceRegistry
{
private:
    static constexpr const char *TAG = "DeviceRegistry";
    static constexpr const char *STORAGE_KEY = "dev_list";
    using ListType = DeviceListStorage<32>;

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

    bool save(const Device &device)
    {
        ensureLoaded();

        int foundIndex = -1;
        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (memcmp(cache.items[i].header.mac, device.header.mac, 6) == 0)
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

    std::optional<Device> getByMac(const uint8_t *mac)
    {
        ensureLoaded();

        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (memcmp(cache.items[i].header.mac, mac, 6) == 0)
            {
                return cache.items[i];
            }
        }

        return std::nullopt;
    }

    bool remove(const uint8_t *mac)
    {
        ensureLoaded();

        if (cache.count == 0)
        {
            return false;
        }

        int foundIndex = -1;
        for (uint8_t i = 0; i < cache.count; ++i)
        {
            if (memcmp(cache.items[i].header.mac, mac, 6) == 0)
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