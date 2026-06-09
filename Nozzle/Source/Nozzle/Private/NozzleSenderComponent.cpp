#include "NozzleSenderComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "NozzleNativeBridge.h"
#include "NozzleRuntimeModule.h"
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#if WITH_NOZZLE_CORE
#include "nozzle/nozzle_c.h"
#endif

struct FNozzleSenderRenderState
{
    FCriticalSection Mutex;
    NozzleSender* SenderHandle = nullptr;
    bool bCancelRequested = false;
    bool bHasPendingDiagnostics = false;
    int64 CompletedRenderSequence = 0;
    FNozzleRuntimeDiagnostics PendingDiagnostics;
    FNozzleMetalIntermediateTextureCache MetalIntermediateCache;
};

namespace
{

void StoreSenderRenderDiagnostics(const TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe>& RenderState, const FNozzleRuntimeDiagnostics& Diagnostics)
{
    if(!RenderState.IsValid())
    {
        return;
    }

    FScopeLock Lock(&RenderState->Mutex);
    RenderState->PendingDiagnostics = Diagnostics;
    RenderState->CompletedRenderSequence += 1;
    RenderState->bHasPendingDiagnostics = true;
}

bool IsSenderRenderCancelled(const TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe>& RenderState)
{
    if(!RenderState.IsValid())
    {
        return true;
    }

    FScopeLock Lock(&RenderState->Mutex);
    return RenderState->bCancelRequested;
}

bool WaitForMetalSourceTextureReady_RenderThread(FRHICommandListImmediate& RHICmdList, FNozzleRuntimeDiagnostics& Diagnostics)
{
    FGPUFenceRHIRef SourceReadyFence = RHICreateGPUFence(TEXT("NozzleMetalSourceReady"));
    if(!SourceReadyFence.IsValid())
    {
        Diagnostics.Message = TEXT("Metal source synchronization failed: RHICreateGPUFence returned null");
        return false;
    }

    RHICmdList.WriteGPUFence(SourceReadyFence);
    RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
    SourceReadyFence->Wait(RHICmdList, RHICmdList.GetGPUMask());
    return true;
}

} // namespace

UNozzleSenderComponent::UNozzleSenderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UNozzleSenderComponent::BeginPlay()
{
    Super::BeginPlay();

    if(bAutoStart)
    {
        StartSender();
    }
}

void UNozzleSenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    DrainRenderThreadDiagnostics();
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

bool UNozzleSenderComponent::DrainRenderThreadDiagnostics()
{
    if(!RenderState.IsValid())
    {
        return false;
    }

    FScopeLock Lock(&RenderState->Mutex);
    if(!RenderState->bHasPendingDiagnostics)
    {
        return false;
    }

    LastRenderDiagnostics = RenderState->PendingDiagnostics;
    LastRenderSequence = RenderState->CompletedRenderSequence;
    LastDiagnostics = LastRenderDiagnostics;
    RenderState->bHasPendingDiagnostics = false;
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
    RenderState = MakeShared<FNozzleSenderRenderState, ESPMode::ThreadSafe>();
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
    TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe> LocalRenderState = RenderState;
    if(LocalRenderState.IsValid())
    {
        {
            FScopeLock Lock(&LocalRenderState->Mutex);
            LocalRenderState->bCancelRequested = true;
        }
        FlushRenderingCommands();

        FNozzleMetalIntermediateTextureCache MetalIntermediateCacheToRelease;

        NozzleSender* SenderToDestroy = nullptr;
        {
            FScopeLock Lock(&LocalRenderState->Mutex);
            SenderToDestroy = LocalRenderState->SenderHandle;
            LocalRenderState->SenderHandle = nullptr;
            MetalIntermediateCacheToRelease = LocalRenderState->MetalIntermediateCache;
            LocalRenderState->MetalIntermediateCache = FNozzleMetalIntermediateTextureCache{};
        }
        if(MetalIntermediateCacheToRelease.Texture != nullptr || MetalIntermediateCacheToRelease.Surface != nullptr || MetalIntermediateCacheToRelease.CommandQueue != nullptr)
        {
            FNozzleNativeBridge::ReleaseMetalIntermediateTextureCache_RenderThread(MetalIntermediateCacheToRelease);
        }
        if(SenderToDestroy != nullptr)
        {
            nozzle_sender_destroy(SenderToDestroy);
        }
    }
    RenderState.Reset();
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
    TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe> LocalRenderState = RenderState;
    if(!LocalRenderState.IsValid())
    {
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PublishFrame blocked: sender render state is not initialized");
        return false;
    }

    FTextureRenderTargetResource* RenderTargetResource = SourceRenderTarget->GameThread_GetRenderTargetResource();
    if(RenderTargetResource == nullptr)
    {
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PublishFrame blocked: render target resource is null");
        return false;
    }

    const FString LocalSenderName = SenderName;
    ENQUEUE_RENDER_COMMAND(NozzleUnrealPublishNativeTexture)(
        [LocalRenderState, LocalSenderName, RenderTargetResource](FRHICommandListImmediate& RHICmdList)
        {
            if(IsSenderRenderCancelled(LocalRenderState))
            {
                return;
            }

            FNozzleNativeTextureView NativeView;
            FNozzleNativeDeviceView DeviceView;
            FNozzleRuntimeDiagnostics RenderThreadDiagnostics;
            if(!FNozzleNativeBridge::CaptureNativeTextureAndDevice_RenderThread(RenderTargetResource, NativeView, DeviceView, RenderThreadDiagnostics))
            {
                StoreSenderRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
                UE_LOG(LogNozzle, Warning, TEXT("platform native sender publish skipped: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            NozzleSender* LocalSenderHandle = nullptr;
            {
                FScopeLock Lock(&LocalRenderState->Mutex);
                LocalSenderHandle = LocalRenderState->SenderHandle;
            }

            if(LocalSenderHandle == nullptr)
            {
                const int32 CreateError = FNozzleNativeBridge::CreateSenderForNativeDevice_RenderThread(LocalSenderName, DeviceView, LocalSenderHandle, RenderThreadDiagnostics);
                if(CreateError != 0 || LocalSenderHandle == nullptr)
                {
                    StoreSenderRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
                    UE_LOG(LogNozzle, Error, TEXT("native-device sender creation failed before publish: %s"), *RenderThreadDiagnostics.Message);
                    return;
                }

                bool bDestroyCreatedSender = false;
                {
                    FScopeLock Lock(&LocalRenderState->Mutex);
                    if(LocalRenderState->bCancelRequested)
                    {
                        bDestroyCreatedSender = true;
                    }
                    else
                    {
                        LocalRenderState->SenderHandle = LocalSenderHandle;
                    }
                }
                if(bDestroyCreatedSender)
                {
                    nozzle_sender_destroy(LocalSenderHandle);
                    return;
                }
            }

            if(IsSenderRenderCancelled(LocalRenderState))
            {
                return;
            }

            if(RenderThreadDiagnostics.bMetalRHI && !WaitForMetalSourceTextureReady_RenderThread(RHICmdList, RenderThreadDiagnostics))
            {
                StoreSenderRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
                UE_LOG(LogNozzle, Error, TEXT("native texture source synchronization failed: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            FNozzleNativeTextureView PublishView;
            if(!FNozzleNativeBridge::PreparePublishTexture_RenderThread(NativeView, DeviceView, LocalRenderState->MetalIntermediateCache, PublishView, RenderThreadDiagnostics))
            {
                StoreSenderRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
                UE_LOG(LogNozzle, Error, TEXT("native texture publish preparation failed: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            const int32 PublishError = FNozzleNativeBridge::PublishNativeTexture_RenderThread(LocalSenderHandle, PublishView, RenderThreadDiagnostics);
            StoreSenderRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
            if(PublishError != 0)
            {
                UE_LOG(LogNozzle, Error, TEXT("native texture publish failed: %s"), *RenderThreadDiagnostics.Message);
            }
        }
    );

    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Width = SourceRenderTarget->SizeX;
    LastDiagnostics.Height = SourceRenderTarget->SizeY;
    LastDiagnostics.Message = TEXT("queued platform native texture publish on the render thread; sender creation uses Unreal native device, and render-thread result is drained on tick/diagnostics query");
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

FNozzleRuntimeDiagnostics UNozzleSenderComponent::GetLastDiagnostics()
{
    DrainRenderThreadDiagnostics();
    return LastDiagnostics;
}

FNozzleRuntimeDiagnostics UNozzleSenderComponent::GetLastRenderDiagnostics()
{
    DrainRenderThreadDiagnostics();
    return LastRenderDiagnostics;
}

int64 UNozzleSenderComponent::GetLastRenderSequence()
{
    DrainRenderThreadDiagnostics();
    return LastRenderSequence;
}
