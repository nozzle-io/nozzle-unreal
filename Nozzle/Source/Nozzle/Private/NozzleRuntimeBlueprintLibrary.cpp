#include "NozzleRuntimeBlueprintLibrary.h"

#include "NozzleNativeBridge.h"
#include "NozzleRuntimeModule.h"

#if WITH_NOZZLE_CORE
#include "Containers/StringConv.h"
#include "nozzle/nozzle_c.h"
#endif

FNozzleRuntimeDiagnostics UNozzleRuntimeBlueprintLibrary::GetNozzleRuntimeDiagnostics()
{
    return FNozzleNativeBridge::MakeRuntimeDiagnostics();
}

TArray<FString> UNozzleRuntimeBlueprintLibrary::EnumerateNozzleSenders(FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    TArray<FString> SenderNames;
    OutDiagnostics = FNozzleNativeBridge::MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return SenderNames;
    }

#if WITH_NOZZLE_CORE
    NozzleSenderInfoArray SenderArray{};
    const NozzleErrorCode Error = nozzle_enumerate_senders(&SenderArray);
    if(Error != NOZZLE_OK)
    {
        OutDiagnostics.State = ENozzleRuntimeState::Error;
        OutDiagnostics.bCanUseRuntime = false;
        OutDiagnostics.Message = FString::Printf(TEXT("nozzle_enumerate_senders failed with error code %d"), static_cast<int32>(Error));
        return SenderNames;
    }

    for(uint32 SenderIndex = 0; SenderIndex < SenderArray.count; SenderIndex++)
    {
        const NozzleSenderInfo& SenderInfo = SenderArray.items[SenderIndex];
        if(SenderInfo.name != nullptr)
        {
            SenderNames.Add(UTF8_TO_TCHAR(SenderInfo.name));
        }
    }
    nozzle_free_sender_info_array(&SenderArray);

    OutDiagnostics.Message = FString::Printf(TEXT("enumerated %d nozzle sender(s)"), SenderNames.Num());
#else
    OutDiagnostics.State = ENozzleRuntimeState::Unavailable;
    OutDiagnostics.bCanUseRuntime = false;
    OutDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: sender discovery is disabled");
#endif

    return SenderNames;
}
