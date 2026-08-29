#include "EspNowManager.h"
#include <WiFiType.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>

static const char *TAG = "ESP_NOW_MANAGER";

constexpr const char *NVS_NAMESPACE = "esp_net";
constexpr const char *MAC_KEY = "custom_mac";
RTC_DATA_ATTR static uint8_t rtc_cached_mac[6];
RTC_DATA_ATTR static bool rtc_mac_valid = false;

EspNowManager *EspNowManager::instance = nullptr;

EspNowManager::EspNowManager(wifi_mode_t wifi_mode, wifi_power_t wifi_power) : wifiMode(wifi_mode), wifiPower(wifi_power)
{
    instance = this;
}

void EspNowManager::setWifiPower(wifi_power_t wifi_power)
{
    wifiPower = wifi_power;
    WiFi.setTxPower(wifi_power);
}

void EspNowManager::setWifiPower(uint8_t wifi_power)
{
    setWifiPower(static_cast<wifi_power_t>(wifi_power));
}

bool EspNowManager::begin()
{
    WiFi.mode(wifiMode);

    uint8_t macToApply[6];
    bool hasMacToApply = false;

    if (rtc_mac_valid)
    {
        memcpy(macToApply, rtc_cached_mac, 6);
        hasMacToApply = true;
        LOG_I(TAG, "Restored MAC from RTC RAM");
    }
    else
    {
        LOG_I(TAG, "Restored MAC from NVS");
        Preferences prefs;
        if (prefs.begin(NVS_NAMESPACE, true))
        {
            size_t macLen = prefs.getBytes(MAC_KEY, macToApply, sizeof(macToApply));
            prefs.end();

            if (macLen == 6)
            {
                memcpy(rtc_cached_mac, macToApply, 6);
                rtc_mac_valid = true;
                hasMacToApply = true;
                LOG_I(TAG, "Restored MAC from NVS Flash");
            }
        }
        else {
            LOG_E(TAG, "Error restoring MAC from NVS");
        }
    }

    if (hasMacToApply)
    {
        esp_wifi_stop();
        esp_wifi_set_mac(WIFI_IF_STA, macToApply);
        esp_wifi_start();

        LOG_I(TAG, "Active MAC: %02X:%02X:%02X:%02X:%02X:%02X",
              macToApply[0], macToApply[1], macToApply[2], macToApply[3], macToApply[4], macToApply[5]);
    }

    if (esp_now_init() != ESP_OK)
    {
        LOG_E(TAG, "Error initializing ESP-NOW");
        return false;
    }

    WiFi.setTxPower(wifiPower);

    esp_now_register_send_cb(OnDataSendStatic);
    esp_now_register_recv_cb(OnDataRecvStatic);

    LOG_I(TAG, "ESP-NOW initialized successfully");
    return true;
}

bool EspNowManager::addPeer(const MacAddress &mac, uint8_t channel)
{
    if (esp_now_is_peer_exist(mac.bytes))
    {
        LOG_D(TAG, "Peer %s is already added", mac.toString().c_str());
        return true;
    }
        

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac.bytes, 6);
    peerInfo.channel = channel;
    peerInfo.ifidx = (wifiMode == WIFI_MODE_AP) ? WIFI_IF_AP : WIFI_IF_STA;
    peerInfo.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peerInfo);
    if(err == ESP_OK) {
        LOG_D(TAG, "Peer %s was successfully added", mac.toString().c_str());
        return true;
    }
    else 
    {
        LOG_E(TAG, "Failed to add a peer: %s (%s)", mac.toString().c_str(), esp_err_to_name(err));
        return false;
    }
}

bool EspNowManager::removePeer(const MacAddress &mac)
{
    if (!esp_now_is_peer_exist(mac.bytes))
        return true;

    return (esp_now_del_peer(mac.bytes) == ESP_OK);
}

/// @brief Generate new mac adress and save it to memory
/// @note To apply new mac reload device (ESP.reload()) or call applyMacAddress function
bool EspNowManager::randomizeMacAddress()
{
    uint8_t newMac[6];
    esp_fill_random(newMac, 6);
    newMac[0] = (newMac[0] & 0xFE) | 0x02;

    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false))
    {
        prefs.putBytes(MAC_KEY, newMac, sizeof(newMac));
        prefs.end();
        
        memcpy(rtc_cached_mac, newMac, 6);
        rtc_mac_valid = true;

        LOG_I(TAG, "New MAC saved to NVS and cached in RTC RAM");
        return true;
    }
    
    LOG_E(TAG, "Failed to open NVS namespace for writing!");
    return false;
}

/// @brief Apply mac address from memory.
bool EspNowManager::applyMacAddress()
{
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true))
    {
        LOG_E(TAG, "Failed to open NVS namespace for reading!");
        return false;
    }

    uint8_t savedMac[6];
    size_t macLen = prefs.getBytes(MAC_KEY, savedMac, sizeof(savedMac));
    prefs.end();

    if (macLen != 6)
    {
        LOG_E(TAG, "Failed to read valid mac address from NVS");
        return false;
    }

    memcpy(rtc_cached_mac, savedMac, 6);
    rtc_mac_valid = true;

    esp_now_deinit();
    esp_wifi_stop();

    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, savedMac);

    esp_wifi_start();
    if (esp_now_init() != ESP_OK)
    {
        LOG_E(TAG, "Failed to re-initialize ESP-NOW");
        return false;
    }

    WiFi.setTxPower(wifiPower);
    esp_now_register_send_cb(OnDataSendStatic);
    esp_now_register_recv_cb(OnDataRecvStatic);

    if (err == ESP_OK)
    {
        LOG_I(TAG, "Applied MAC: %02X:%02X:%02X:%02X:%02X:%02X",
              savedMac[0], savedMac[1], savedMac[2], savedMac[3], savedMac[4], savedMac[5]);
        return true;
    }

    LOG_E(TAG, "Failed to apply MAC address: 0x%X", err);
    return false;
}

MacAddress EspNowManager::getMacAddress() const
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    return MacAddress(mac);
}

void EspNowManager::OnDataSendStatic(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        LOG_I(TAG, "Data sent successfully");
    }
    else
    {
        LOG_E(TAG, "Failed to send data");
    }
}

void EspNowManager::OnDataRecvStatic(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len)
{
    if (instance)
        instance->handleReceive(esp_now_info, data, len);
}

void EspNowManager::handleReceive(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len)
{
    if (len < sizeof(PacketHeader))
        return;

    const PacketHeader *header = reinterpret_cast<const PacketHeader *>(data);

    auto it = handlers.find(header->type);
    if (it != handlers.end())
    {
        RawPacketHandler handlerCopy = it->second;
        handlerCopy(esp_now_info, data, len);
    }
}