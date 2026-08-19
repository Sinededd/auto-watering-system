#include "StartState.h"
#include <Logger.h>
#include "WorkingState.h"
#include "PairingState.h"
#include "DeviceContext.h"

void StartState::onEnter(DeviceContext &ctx)
{
    LOG_I(TAG, "==== Start sensor ====");

    auto netCfg = ctx.getStorage().load<NetworkConfig>("net_cfg", ctx.getRtcNetCache());

    if (netCfg.has_value() && netCfg->isPaired)
    {
        ctx.transitionTo(std::make_unique<WorkingState>());
    }
    else
    {
        ctx.transitionTo(std::make_unique<PairingState>());
    }
}