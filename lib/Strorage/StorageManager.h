#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <type_traits>
#include <optional>
#include <Logger.h>

template <typename T>
struct RtcCache
{
    static constexpr uint32_t RTC_MAGIC = 0x5A5A1234;

    uint32_t magic;
    T data;

    bool isValid() const { return magic == RTC_MAGIC; }
    void validate() { magic = RTC_MAGIC; }
    void invalidate() { magic = 0; }
};

class StorageManager
{
private:
    static constexpr const char *TAG = "Storage";
    Preferences prefs;
    const char *nsName;

public:
    explicit StorageManager(const char *namespaceName = "app_cfg")
        : nsName(namespaceName) {}

    /// @brief Save data directly to RTC memory (persists during deep sleep).
    /// @param rtcCache Need to declarate 'static RTC_DATA_ATTR RtcCache<T> savedVarible' and transfer it to function.
    template <typename T>
    bool saveRtc(const T &value, RtcCache<T> &rtcCache)
    {
        rtcCache.data = value;
        rtcCache.validate();
        LOG_D(TAG, "Data saved directly to RTC SRAM (%u bytes)", (unsigned)sizeof(T));
        return true;
    }

    /// @brief Load data directly from RTC memory.
    /// @param rtcCache Need to declarate 'static RTC_DATA_ATTR RtcCache<T> savedVarible' and transfer it to function.
    /// @return std::optional<T> or std::nullopt if data is invalid.
    template <typename T>
    std::optional<T> loadRtc(const RtcCache<T> &rtcCache)
    {
        if (rtcCache.isValid())
        {
            LOG_D(TAG, "Data loaded from RTC SRAM (%u bytes)", (unsigned)sizeof(T));
            return rtcCache.data;
        }
        LOG_D(TAG, "RTC cache is invalid or uninitialized");
        return std::nullopt;
    }

    /// @brief Save data to NVS memory with optional RTC caching.
    /// @param key Key name in NVS (MAX 15 characters for ESP32!).
    /// @param rtcCache Need to declarate 'static RTC_DATA_ATTR RtcCache<T> savedVarible' and transfer it to function.
    template <typename T>
    bool save(const char *key, const T &value, RtcCache<T> *rtcCache = nullptr)
    {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable!");

        if (!prefs.begin(nsName, false))
        {
            LOG_E(TAG, "Failed to open NVS namespace '%s' in RW mode", nsName);
            return false;
        }

        size_t written = prefs.putBytes(key, &value, sizeof(T));
        prefs.end();

        bool success = (written == sizeof(T));

        if (success)
        {
            LOG_I(TAG, "Key '%s' saved to NVS (%u bytes)", key, (unsigned)written);
            if (rtcCache != nullptr)
            {
                rtcCache->data = value;
                rtcCache->validate();
                LOG_D(TAG, "Key '%s' cached to RTC SRAM", key);
            }
        }
        else
        {
            LOG_E(TAG, "Failed to save key '%s' to NVS. Wrote %u of %u bytes", key, (unsigned)written, (unsigned)sizeof(T));
        }

        return success;
    }

    /// @brief Load data from NVS memory or RTC cache if valid.
    /// @param key Key name in NVS (MAX 15 characters for ESP32!).
    /// @param rtcCache Need to declarate 'static RTC_DATA_ATTR RtcCache<T> savedVarible' and transfer it to function.
    /// @return std::optional<T> or std::nullopt if data does not exist.
    template <typename T>
    std::optional<T> load(const char *key, RtcCache<T> *rtcCache = nullptr)
    {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable!");

        if (rtcCache != nullptr && rtcCache->isValid())
        {
            LOG_D(TAG, "RTC cache hit for key '%s'", key);
            return rtcCache->data;
        }

        if (!prefs.begin(nsName, true))
        {
            LOG_W(TAG, "NVS namespace '%s' not found or not initialized", nsName);
            return std::nullopt;
        }

        bool exists = prefs.isKey(key);
        T loadedValue{};

        if (exists)
        {
            size_t readBytes = prefs.getBytes(key, &loadedValue, sizeof(T));
            if (readBytes == sizeof(T))
            {
                LOG_D(TAG, "Key '%s' loaded from NVS Flash (%u bytes)", key, (unsigned)readBytes);
            }
            else
            {
                LOG_E(TAG, "Key '%s' size mismatch! Expected %u, got %u bytes", key, (unsigned)sizeof(T), (unsigned)readBytes);
                exists = false;
            }
        }
        else
        {
            LOG_W(TAG, "Key '%s' not found in NVS namespace '%s'", key, nsName);
        }

        prefs.end();

        if (exists)
        {
            if (rtcCache != nullptr)
            {
                rtcCache->data = loadedValue;
                rtcCache->validate();
                LOG_D(TAG, "Key '%s' cached into RTC SRAM", key);
            }
            return loadedValue;
        }

        return std::nullopt;
    }

    /// @brief Invalidate RTC memory cache.
    template <typename T>
    void invalidate(RtcCache<T> &rtcCache)
    {
        rtcCache.invalidate();
        LOG_D(TAG, "RTC cache invalidated");
    }

    /// @brief Remove data from NVS memory and optionally invalidate RTC cache.
    template <typename T>
    bool remove(const char *key, RtcCache<T> *rtcCache = nullptr)
    {
        if (rtcCache != nullptr)
        {
            rtcCache->invalidate();
            LOG_D(TAG, "RTC cache invalidated for key '%s'", key);
        }

        return remove(key);
    }

    /// @brief Remove data from NVS memory without RTC cache pointer.
    bool remove(const char *key)
    {
        if (!prefs.begin(nsName, false))
        {
            LOG_E(TAG, "Failed to open NVS namespace '%s' to remove key '%s'", nsName, key);
            return false;
        }

        bool result = prefs.remove(key);
        prefs.end();

        if (result)
        {
            LOG_I(TAG, "Key '%s' removed from NVS namespace '%s'", key, nsName);
        }
        else
        {
            LOG_W(TAG, "Failed to remove key '%s' from NVS (key might not exist)", key);
        }

        return result;
    }
};