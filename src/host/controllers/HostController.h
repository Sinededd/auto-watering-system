#pragma once
#include <EspNowManager.h>
#include <Logger.h>
#include "DeviceController.h"

class HostController
{
private:
    EspNowManager &net;
    DeviceController &devController;

    void handlePairRequest(const esp_now_recv_info_t *info, const PairRequestPacket &packet)
    {
        devController.onPairRequest(info, packet);
    }

public:
    HostController(EspNowManager &netMgr, DeviceController &devController)
        : net(netMgr), devController(devController) {}

    void begin()
    {
        net.onPacket<PairRequestPacket>(
            PacketType::PAIR_REQUEST,
            [this](const esp_now_recv_info_t *info, const PairRequestPacket &pkt)
            {
                this->handlePairRequest(info, pkt);
            });
    }
};