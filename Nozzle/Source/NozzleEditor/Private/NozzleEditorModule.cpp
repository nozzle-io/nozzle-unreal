#include "NozzleEditorModule.h"

#include "Modules/ModuleManager.h"
#include "NozzleRuntimeModule.h"

IMPLEMENT_MODULE(FNozzleEditorModule, NozzleEditor)

void FNozzleEditorModule::StartupModule()
{
    UE_LOG(LogNozzle, Display, TEXT("Nozzle Unreal editor scaffold loaded. Editor diagnostics UI is not implemented yet."));
}

void FNozzleEditorModule::ShutdownModule()
{
}
