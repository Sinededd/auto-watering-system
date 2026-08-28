#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <map>
#include <WiFiType.h>
#include <Logger.h>
#include "Packets.h"
#include <WiFiGeneric.h>
#include <MacAddress.h>

using RawPacketHandler = std::function<void(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len)>;

class EspNowManager
{
private:
    static EspNowManager *instance;
    std::map<PacketType, RawPacketHandler> handlers;

    wifi_mode_t wifiMode;
    wifi_power_t wifiPower;

    static void OnDataSendStatic(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
    static void OnDataRecvStatic(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len);

    /// @brief Internal function to handle received data.
    void handleReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len);

public:
    EspNowManager(wifi_mode_t wifi_mode = WIFI_STA, wifi_power_t wifi_power = WIFI_POWER_19_5dBm);

    bool begin();

    wifi_power_t getWifiPower() { return wifiPower; }
    void setWifiPower(wifi_power_t wifi_power);
    void setWifiPower(uint8_t wifi_power);

    bool addPeer(const MacAddress &mac, uint8_t channel = 0);
    bool removePeer(const MacAddress &mac);

    /// @brief Changing MAC addresses to random with ESP-NOW restarting
    bool randomizeMacAddress();
    bool applyMacAddress();

    /// @brief Get the current MAC address of your device
    MacAddress getMacAddress() const;

    /// @brief Register a handler for a specific packet structure
    template <typename PacketStruct>
    void onPacket(PacketType type, std::function<void(const esp_now_recv_info_t *esp_now_info, const PacketStruct &packet)> cb)
    {
        LOG_D("ESP_NOW_MANAGER", "Register handler for packet type: %d, size: %zu bytes", static_cast<int>(type), sizeof(PacketStruct));
        handlers[type] = [cb](const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len)
        {
            if (len == sizeof(PacketStruct))
            {
                const PacketStruct *packet = reinterpret_cast<const PacketStruct *>(data);
                cb(esp_now_info, *packet);
            }
            else
            {
                LOG_W("ESP_NOW_MANAGER", "Packet size mismatch! Expected: %zu, Received: %d", sizeof(PacketStruct), len);
            }
        };
    }

    /// @brief Remove a handler for a specific packet structure
    void removePacketHandler(PacketType type)
    {
        handlers.erase(type);
        LOG_D("ESP_NOW_MANAGER", "Removed handler for packet type: %d", static_cast<int>(type));
    }

    /// @brief Send any packet structure
    template <typename T>
    bool send(const MacAddress &mac, const T &packet)
    {
        static_assert(sizeof(T) <= 250, "Packet size exceeds ESP-NOW limit of 250 bytes!");
        LOG_D("ESP_NOW_MANAGER", "Sending packet to MAC: %s, size: %zu bytes",
              mac.toString().c_str(),
              sizeof(T));

        esp_err_t err = esp_now_send(mac.bytes, (const uint8_t *)&packet, sizeof(T));
        if (err != ESP_OK) {
            LOG_E("ESP_NOW_MANAGER", "esp_now_send failed to %s! Error code: 0x%X", 
                mac.toString().c_str(), err);
            return false;
        }
        return true;
    }
};
