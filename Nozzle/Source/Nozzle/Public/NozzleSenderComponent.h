#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NozzleDiagnostics.h"
#include "NozzleSenderComponent.generated.h"

class UTextureRenderTarget2D;
struct NozzleSender;

UCLASS(ClassGroup = (Nozzle), BlueprintType, meta = (BlueprintSpawnableComponent))
class NOZZLE_API UNozzleSenderComponent final : public UActorComponent
{
    GENERATED_BODY()

public:
    UNozzleSenderComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    FString SenderName = TEXT("UnrealNozzleSender");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    TObjectPtr<UTextureRenderTarget2D> SourceRenderTarget = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nozzle")
    bool bAutoStart = false;

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Sender")
    bool StartSender();

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Sender")
    void StopSender();

    UFUNCTION(BlueprintCallable, Category = "Nozzle|Sender")
    bool PublishFrame();

    UFUNCTION(BlueprintPure, Category = "Nozzle|Sender")
    bool IsSenderRunning() const;

    UFUNCTION(BlueprintPure, Category = "Nozzle|Diagnostics")
    FNozzleRuntimeDiagnostics GetLastDiagnostics() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    NozzleSender* SenderHandle = nullptr;
    bool bSenderRunning = false;
    FNozzleRuntimeDiagnostics LastDiagnostics;

    bool RefreshRuntimeReadiness(const TCHAR* OperationName);
};
