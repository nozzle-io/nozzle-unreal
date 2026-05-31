#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NozzleDiagnostics.h"
#include "NozzleRuntimeBlueprintLibrary.generated.h"

UCLASS()
class NOZZLE_API UNozzleRuntimeBlueprintLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Nozzle|Diagnostics")
    static FNozzleRuntimeDiagnostics GetNozzleRuntimeDiagnostics();

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Discovery")
    static TArray<FString> EnumerateNozzleSenders(FNozzleRuntimeDiagnostics& OutDiagnostics);
};
