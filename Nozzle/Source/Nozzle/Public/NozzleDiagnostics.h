#pragma once

#include "CoreMinimal.h"
#include "NozzleDiagnostics.generated.h"

UENUM(BlueprintType)
enum class ENozzleRuntimeState : uint8
{
    Unavailable UMETA(DisplayName = "Unavailable"),
    UnsupportedRHI UMETA(DisplayName = "Unsupported RHI"),
    Ready UMETA(DisplayName = "Ready"),
    Running UMETA(DisplayName = "Running"),
    Error UMETA(DisplayName = "Error")
};

USTRUCT(BlueprintType)
struct NOZZLE_API FNozzleRuntimeDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    ENozzleRuntimeState State = ENozzleRuntimeState::Unavailable;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    bool bWithNozzleCore = false;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    bool bSupportedPlatform = false;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    bool bD3D11RHI = false;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    bool bMetalRHI = false;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    bool bCanUseRuntime = false;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    FString SelectedRHI;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    FString Backend;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    FString Format;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    FString TransferMode;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    int32 Width = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Nozzle")
    int32 Height = 0;
};
