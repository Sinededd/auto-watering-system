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

public:
    explicit DeviceRegistry(StorageManager &storageMgr) : storage(storageMgr) {}

    bool save(const Device &device)
    {
        ListType listData = storage.load<ListType>(STORAGE_KEY).value_or(ListType{});

        int foundIndex = -1;
        for (uint8_t i = 0; i < listData.count; ++i)
        {
            if (memcmp(listData.items[i].header.mac, device.header.mac, 6) == 0)
            {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex >= 0)
        {
            listData.items[foundIndex] = device;
        }
        else
        {
            if (listData.count >= 32)
            {
                return false;
            }
            listData.items[listData.count] = device;
            listData.count++;
        }

        return storage.save(STORAGE_KEY, listData);
    }

    std::optional<Device> getByMac(const uint8_t *mac)
    {
        std::optional<ListType> listData = storage.load<ListType>(STORAGE_KEY);
        if (!listData.has_value())
        {
            return std::nullopt;
        }

        for (uint8_t i = 0; i < listData->count; ++i)
        {
            if (memcmp(listData->items[i].header.mac, mac, 6) == 0)
            {
                return listData->items[i];
            }
        }

        return std::nullopt;
    }

    std::optional<ListType> getAll()
    {
        return storage.load<ListType>(STORAGE_KEY);
    }
};