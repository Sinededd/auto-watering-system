#pragma once
#include <EspNowManager.h>
#include <Logger.h>
#include "PairingBuffer.h"
#include "host/events/EventBus.h"

class HostController
{
private:
    EventBus& eventBus;
    EspNowManager &net;
    PairingBuffer &pairingBuffer;

    void handlePairRequest(const esp_now_recv_info_t *info, const PairRequestPacket &packet)
    {
        pairingBuffer.onPairRequest(info->src_addr, packet.deviceType);
    }

public:
    HostController(EspNowManager &netMgr, EventBus &bus, PairingBuffer &pairingBuffer)
        : eventBus(bus), net(netMgr), pairingBuffer(pairingBuffer) {}

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