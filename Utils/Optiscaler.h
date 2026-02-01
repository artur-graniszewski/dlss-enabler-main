#pragma once
#include <Wtypes.h>

struct OptiScalerConfig {
    bool spoof_as_enabler = true;
};
 
bool OptiScaler_Init(HMODULE self, const OptiScalerConfig* cfg);
void OptiScaler_Shutdown(); 
bool BeginVersionBypassHooks();
