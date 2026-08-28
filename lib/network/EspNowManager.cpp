#include "EspNowManager.h"
#include <WiFiType.h>
#include <WiFi.h>

static const char *TAG = "ESP_NOW_MANAGER";

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
        return true;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac.bytes, 6);
    peerInfo.channel = channel;
    peerInfo.ifidx = (wifiMode == WIFI_MODE_AP) ? WIFI_IF_AP : WIFI_IF_STA;
    peerInfo.encrypt = false;

    return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

bool EspNowManager::removePeer(const MacAddress &mac)
{
    if (!esp_now_is_peer_exist(mac.bytes))
        return true;

    return (esp_now_del_peer(mac.bytes) == ESP_OK);
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