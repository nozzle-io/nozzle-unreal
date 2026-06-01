#include "NozzleSenderComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "NozzleNativeBridge.h"
#include "NozzleRuntimeModule.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#if WITH_NOZZLE_CORE
#include "Containers/StringConv.h"
#include "nozzle/nozzle_c.h"
#endif

UNozzleSenderComponent::UNozzleSenderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNozzleSenderComponent::BeginPlay()
{
    Super::BeginPlay();

    if(bAutoStart)
    {
        StartSender();
    }
}

void UNozzleSenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSender();
    Super::EndPlay(EndPlayReason);
}

bool UNozzleSenderComponent::RefreshRuntimeReadiness(const TCHAR* OperationName)
{
    LastDiagnostics = FNozzleNativeBridge::MakeRuntimeDiagnostics();
    if(!LastDiagnostics.bCanUseRuntime)
    {
        LastDiagnostics.Message = FString::Printf(TEXT("%s blocked: %s"), OperationName, *LastDiagnostics.Message);
        UE_LOG(LogNozzle, Warning, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }
    return true;
}

bool UNozzleSenderComponent::StartSender()
{
    if(bSenderRunning)
    {
        return true;
    }

    if(!RefreshRuntimeReadiness(TEXT("StartSender")))
    {
        return false;
    }

#if WITH_NOZZLE_CORE
    bSenderRunning = true;
    LastDiagnostics.State = ENozzleRuntimeState::Ready;
    LastDiagnostics.Message = TEXT("nozzle sender armed; native-device sender creation is deferred to the render thread on first publish");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: StartSender cannot create a native nozzle sender");
    UE_LOG(LogNozzle, Warning, TEXT("%s"), *LastDiagnostics.Message);
    return false;
#endif
}

void UNozzleSenderComponent::StopSender()
{
#if WITH_NOZZLE_CORE
    if(SenderHandle != nullptr)
    {
        FlushRenderingCommands();
        nozzle_sender_destroy(SenderHandle);
        SenderHandle = nullptr;
    }
#endif
    bSenderRunning = false;
}

bool UNozzleSenderComponent::PublishFrame()
{
    if(SourceRenderTarget == nullptr)
    {
        LastDiagnostics = FNozzleNativeBridge::MakeRuntimeDiagnostics();
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PublishFrame blocked: SourceRenderTarget is null");
        UE_LOG(LogNozzle, Warning, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }

    if(!bSenderRunning && !StartSender())
    {
        return false;
    }

    if(!RefreshRuntimeReadiness(TEXT("PublishFrame")))
    {
        return false;
    }

#if WITH_NOZZLE_CORE
    FTextureRenderTargetResource* RenderTargetResource = SourceRenderTarget->GameThread_GetRenderTargetResource();
    if(RenderTargetResource == nullptr)
    {
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PublishFrame blocked: render target resource is null");
        return false;
    }

    UNozzleSenderComponent* Component = this;
    const FString LocalSenderName = SenderName;
    ENQUEUE_RENDER_COMMAND(NozzleUnrealPublishNativeTexture)(
        [Component, LocalSenderName, RenderTargetResource](FRHICommandListImmediate& RHICmdList)
        {
            FNozzleNativeTextureView NativeView;
            FNozzleNativeDeviceView DeviceView;
            FNozzleRuntimeDiagnostics RenderThreadDiagnostics;
            if(!FNozzleNativeBridge::CaptureNativeTextureAndDevice_RenderThread(RenderTargetResource, NativeView, DeviceView, RenderThreadDiagnostics))
            {
                UE_LOG(LogNozzle, Warning, TEXT("platform native sender publish skipped: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            const int32 CreateError = FNozzleNativeBridge::CreateSenderForNativeDevice_RenderThread(LocalSenderName, DeviceView, Component->SenderHandle, RenderThreadDiagnostics);
            if(CreateError != 0 || Component->SenderHandle == nullptr)
            {
                UE_LOG(LogNozzle, Error, TEXT("native-device sender creation failed before publish: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            const int32 PublishError = FNozzleNativeBridge::PublishNativeTexture_RenderThread(Component->SenderHandle, NativeView, RenderThreadDiagnostics);
            if(PublishError != 0)
            {
                UE_LOG(LogNozzle, Error, TEXT("native texture publish failed: %s"), *RenderThreadDiagnostics.Message);
            }
        }
    );

    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Width = SourceRenderTarget->SizeX;
    LastDiagnostics.Height = SourceRenderTarget->SizeY;
    LastDiagnostics.Message = TEXT("queued platform native texture publish on the render thread; sender creation uses Unreal native device, but CI remains static until Unreal Engine executes this path");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: PublishFrame cannot use the native texture publish bridge");
    return false;
#endif
}

bool UNozzleSenderComponent::IsSenderRunning() const
{
    return bSenderRunning;
}

FNozzleRuntimeDiagnostics UNozzleSenderComponent::GetLastDiagnostics() const
{
    return LastDiagnostics;
}
