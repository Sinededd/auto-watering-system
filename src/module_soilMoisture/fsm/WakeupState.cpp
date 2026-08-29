#include "WakeupState.h"
#include <Logger.h>
#include "DeviceContext.h"
#include "PairingState.h"
#include "WorkingState.h"

void WakeupState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "Entered WAKEUP state.");
    ctx.getLed().turnOn();
    auto netCfg = ctx.getStorage().load<NetworkConfig>("net_cfg");
    if (netCfg.has_value())
    {
        this->hostMAC = netCfg->hostMac;
        ctx.getNet().addPeer(netCfg->hostMac, netCfg->channel);
        ctx.getTelemetryController().sendTelemetry(netCfg->hostMac);

        lastSentTel = millis();
        countSendTel = 1;
    }
    else
    {
        LOG_E(TAG, "Failed to add host peer. Switch to Pairing state!");
        ctx.transitionTo(std::make_unique<PairingState>());
    }
}

void WakeupState::onButtonPressed(DeviceContext &ctx)
{
    LOG_I(TAG, "Button pressed! Resetting telemetry sequence.");

    ctx.getTelemetryController().sendTelemetry(hostMAC);
    lastSentTel = millis();
    countSendTel = 1;
}

void WakeupState::update(DeviceContext &ctx)
{
    uint32_t now = millis();

    if (now - lastSentTel > 3000)
    {
        if (countSendTel > 2)
        {
            ctx.transitionTo(std::make_unique<WorkingState>());
            return;
        }

        ctx.getTelemetryController().sendTelemetry(hostMAC);
        lastSentTel = now;
        countSendTel++;
    }
}
