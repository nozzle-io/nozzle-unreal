#include "NozzleRuntimeModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogNozzle);
IMPLEMENT_MODULE(FNozzleRuntimeModule, Nozzle)

void FNozzleRuntimeModule::StartupModule()
{
    UE_LOG(LogNozzle, Display, TEXT("Nozzle Unreal Phase 0 scaffold loaded. Runtime texture sharing is not implemented or validated yet."));
}

void FNozzleRuntimeModule::ShutdownModule()
{
}
