#include "NozzleEditorModule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FNozzleEditorModule, NozzleEditor)

void FNozzleEditorModule::StartupModule()
{
    UE_LOG(LogTemp, Display, TEXT("Nozzle Unreal editor scaffold loaded. Editor diagnostics UI is not implemented yet."));
}

void FNozzleEditorModule::ShutdownModule()
{
}
