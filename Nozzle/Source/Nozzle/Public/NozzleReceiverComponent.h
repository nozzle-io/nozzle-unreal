#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NozzleDiagnostics.h"
#include "NozzleReceiverComponent.generated.h"

class UTextureRenderTarget2D;
struct NozzleReceiver;

UCLASS(ClassGroup = (Nozzle), BlueprintType, meta = (BlueprintSpawnableComponent))
class NOZZLE_API UNozzleReceiverComponent final : public UActorComponent
{
    GENERATED_BODY()

public:
    UNozzleReceiverComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    FString SenderName = TEXT("UnrealNozzleSender");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    TObjectPtr<UTextureRenderTarget2D> TargetRenderTarget = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    bool bAutoStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle", meta = (ClampMin = "0"))
    int32 AcquireTimeoutMs = 0;

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Receiver")
    bool StartReceiver();

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Receiver")
    void StopReceiver();

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Receiver")
    bool PollFrame();

    UFUNCTION(BlueprintPure, Category = "Nozzle|Receiver")
    bool IsReceiverRunning() const;

    UFUNCTION(BlueprintPure, Category = "Nozzle|Diagnostics")
    FNozzleRuntimeDiagnostics GetLastDiagnostics() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    NozzleReceiver* ReceiverHandle = nullptr;
    bool bReceiverRunning = false;
    FNozzleRuntimeDiagnostics LastDiagnostics;

    bool RefreshRuntimeReadiness(const TCHAR* OperationName);
};
