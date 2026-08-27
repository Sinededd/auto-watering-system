#include "WorkingState.h"
#include "PairingState.h"
#include "DeviceContext.h"
#include <Logger.h>

void WorkingState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered WORKING state.");

    auto netCfg = ctx.getStorage().load<NetworkConfig>("net_cfg", ctx.getRtcNetCache());
    if (netCfg.has_value())
    {
        ctx.getNet().addPeer(netCfg->hostMac, netCfg->channel);

        sendTelemetryAndSleep(ctx, netCfg->hostMac);
    }
    else
    {
        LOG_E(TAG, "Failed to add host peer. Switch to Pairing state!");
        ctx.transitionTo(std::make_unique<PairingState>());
    }
}

void WorkingState::onButtonPressed(DeviceContext &ctx)
{
}

void WorkingState::sendTelemetryAndSleep(DeviceContext &ctx, const MacAddress &hostMac)
{
    // TelemetryPacket packet{  };
    // ctx.getNet().send(hostMac, packet);

    LOG_I(TAG, "Data sent. Going to Deep Sleep");
    esp_sleep_enable_timer_wakeup(10 * 1000000ULL);
    Logger::flush();
    esp_deep_sleep_start();
}