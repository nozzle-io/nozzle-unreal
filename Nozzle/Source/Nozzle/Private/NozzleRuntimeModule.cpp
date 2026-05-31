#include "NozzleRuntimeModule.h"

#include "Modules/ModuleManager.h"
#include "NozzleD3D11Bridge.h"

DEFINE_LOG_CATEGORY(LogNozzle);
IMPLEMENT_MODULE(FNozzleRuntimeModule, Nozzle)

void FNozzleRuntimeModule::StartupModule()
{
    const FNozzleRuntimeDiagnostics Diagnostics = FNozzleD3D11Bridge::MakeRuntimeDiagnostics();
    UE_LOG(LogNozzle, Display, TEXT("Nozzle Unreal runtime module loaded. RHI=%s Backend=%s WITH_NOZZLE_CORE=%d Message=%s"), *Diagnostics.SelectedRHI, *Diagnostics.Backend, Diagnostics.bWithNozzleCore ? 1 : 0, *Diagnostics.Message);
}

void FNozzleRuntimeModule::ShutdownModule()
{
}
