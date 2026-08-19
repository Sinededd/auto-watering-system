#pragma once
#include <EspNowManager.h>
#include <Logger.h>
#include "PairingBuffer.h"

class HostController
{
private:
    EspNowManager &net;
    PairingBuffer pairingBuffer;

    void handlePairRequest(const esp_now_recv_info_t *info, const PairRequestPacket &packet)
    {
        pairingBuffer.onPairRequest(info->src_addr, packet.deviceType);
    }

public:
    HostController(EspNowManager &netMgr)
        : net(netMgr) {}

    void begin()
    {
        net.onPacket<PairRequestPacket>(
            PacketType::PAIR_REQUEST,
            [this](const esp_now_recv_info_t *info, const PairRequestPacket &pkt)
            {
                this->handlePairRequest(info, pkt);
            });
    }

    void update() {
        pairingBuffer.update();
    }

    // bool pairDevice(const String& macStr, DeviceType type) {
    // }

    const PairingBuffer& getPairingBuffer() const {
        return pairingBuffer;
    }
};