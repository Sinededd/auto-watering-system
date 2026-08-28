#include "WorkingState.h"
#include "PairingState.h"
#include "DeviceContext.h"
#include <Logger.h>
#include <driver/rtc_io.h>

void WorkingState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered WORKING state.");

    auto netCfg = ctx.getStorage().load<NetworkConfig>("net_cfg", ctx.getRtcNetCache());
    if (netCfg.has_value())
    {
        ctx.getNet().addPeer(netCfg->hostMac, netCfg->channel);

        ctx.getTelemetryController().sendTelemetry(netCfg->hostMac);

        ctx.getSleepController().goToSleep();
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