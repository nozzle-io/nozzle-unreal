#include "NozzleReceiverComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "NozzleNativeBridge.h"
#include "NozzleRuntimeModule.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#if WITH_NOZZLE_CORE
#include "nozzle/nozzle_c.h"
#endif

struct FNozzleReceiverRenderState
{
    FCriticalSection Mutex;
    bool bCancelRequested = false;
    bool bHasPendingDiagnostics = false;
    int64 CompletedRenderSequence = 0;
    FNozzleRuntimeDiagnostics PendingDiagnostics;
};

namespace
{

void StoreReceiverRenderDiagnostics(const TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe>& RenderState, const FNozzleRuntimeDiagnostics& Diagnostics)
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

bool IsReceiverRenderCancelled(const TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe>& RenderState)
{
    if(!RenderState.IsValid())
    {
        return true;
    }

    FScopeLock Lock(&RenderState->Mutex);
    return RenderState->bCancelRequested;
}

} // namespace

UNozzleReceiverComponent::UNozzleReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UNozzleReceiverComponent::BeginPlay()
{
    Super::BeginPlay();

    if(bAutoStart)
    {
        StartReceiver();
    }
}

void UNozzleReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    DrainRenderThreadDiagnostics();
}

void UNozzleReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopReceiver();
    Super::EndPlay(EndPlayReason);
}

bool UNozzleReceiverComponent::RefreshRuntimeReadiness(const TCHAR* OperationName)
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

bool UNozzleReceiverComponent::DrainRenderThreadDiagnostics()
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

bool UNozzleReceiverComponent::StartReceiver()
{
    if(bReceiverRunning)
    {
        return true;
    }

    if(!RefreshRuntimeReadiness(TEXT("StartReceiver")))
    {
        return false;
    }

#if WITH_NOZZLE_CORE
    RenderState = MakeShared<FNozzleReceiverRenderState, ESPMode::ThreadSafe>();
    const int32 Error = FNozzleNativeBridge::CreateReceiverForBackend(SenderName, ReceiverHandle, LastDiagnostics);
    if(Error != 0 || ReceiverHandle == nullptr)
    {
        ReceiverHandle = nullptr;
        RenderState.Reset();
        bReceiverRunning = false;
        UE_LOG(LogNozzle, Error, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }

    bReceiverRunning = true;
    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Message = TEXT("nozzle receiver created through the shared native bridge backend gate");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: StartReceiver cannot create a native nozzle receiver");
    UE_LOG(LogNozzle, Warning, TEXT("%s"), *LastDiagnostics.Message);
    return false;
#endif
}

void UNozzleReceiverComponent::StopReceiver()
{
#if WITH_NOZZLE_CORE
    TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe> LocalRenderState = RenderState;
    if(LocalRenderState.IsValid())
    {
        {
            FScopeLock Lock(&LocalRenderState->Mutex);
            LocalRenderState->bCancelRequested = true;
        }
        FlushRenderingCommands();
    }

    if(ReceiverHandle != nullptr)
    {
        nozzle_receiver_destroy(ReceiverHandle);
        ReceiverHandle = nullptr;
    }
    RenderState.Reset();
#endif
    bReceiverRunning = false;
}

bool UNozzleReceiverComponent::PollFrame()
{
    if(TargetRenderTarget == nullptr)
    {
        LastDiagnostics = FNozzleNativeBridge::MakeRuntimeDiagnostics();
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PollFrame blocked: TargetRenderTarget is null");
        UE_LOG(LogNozzle, Warning, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }

    if(!bReceiverRunning && !StartReceiver())
    {
        return false;
    }

    if(!RefreshRuntimeReadiness(TEXT("PollFrame")))
    {
        return false;
    }

#if WITH_NOZZLE_CORE
    TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe> LocalRenderState = RenderState;
    if(!LocalRenderState.IsValid())
    {
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PollFrame blocked: receiver render state is not initialized");
        return false;
    }

    NozzleAcquireDesc AcquireDesc{};
    AcquireDesc.timeout_ms = static_cast<uint64>(FMath::Max(0, AcquireTimeoutMs));

    NozzleFrame* Frame = nullptr;
    const NozzleErrorCode AcquireError = nozzle_receiver_acquire_frame(ReceiverHandle, &AcquireDesc, &Frame);
    if(AcquireError != NOZZLE_OK || Frame == nullptr)
    {
        LastDiagnostics.State = AcquireError == NOZZLE_ERROR_TIMEOUT ? ENozzleRuntimeState::Running : ENozzleRuntimeState::Error;
        LastDiagnostics.Message = FString::Printf(TEXT("nozzle_receiver_acquire_frame returned error code %d"), static_cast<int32>(AcquireError));
        return false;
    }

    NozzleFrameInfo FrameInfo{};
    const NozzleErrorCode InfoError = nozzle_frame_get_info(Frame, &FrameInfo);
    if(InfoError != NOZZLE_OK)
    {
        nozzle_frame_release(Frame);
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = FString::Printf(TEXT("nozzle_frame_get_info failed with error code %d"), static_cast<int32>(InfoError));
        return false;
    }

    FTextureRenderTargetResource* RenderTargetResource = TargetRenderTarget->GameThread_GetRenderTargetResource();
    if(RenderTargetResource == nullptr)
    {
        nozzle_frame_release(Frame);
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PollFrame blocked: target render target resource is null");
        return false;
    }

    NozzleFrame* FrameForRenderThread = Frame;
    ENQUEUE_RENDER_COMMAND(NozzleUnrealCopyNativeFrame)(
        [LocalRenderState, FrameForRenderThread, RenderTargetResource, Width = FrameInfo.width, Height = FrameInfo.height](FRHICommandListImmediate& RHICmdList)
        {
            if(IsReceiverRenderCancelled(LocalRenderState))
            {
                nozzle_frame_release(FrameForRenderThread);
                return;
            }

            FNozzleNativeTextureView NativeView;
            FNozzleRuntimeDiagnostics RenderThreadDiagnostics;
            if(!FNozzleNativeBridge::CaptureNativeTexture_RenderThread(RenderTargetResource, NativeView, RenderThreadDiagnostics))
            {
                StoreReceiverRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
                UE_LOG(LogNozzle, Warning, TEXT("platform native receiver copy skipped: %s"), *RenderThreadDiagnostics.Message);
                nozzle_frame_release(FrameForRenderThread);
                return;
            }

            NativeView.Width = static_cast<int32>(Width);
            NativeView.Height = static_cast<int32>(Height);
            if(IsReceiverRenderCancelled(LocalRenderState))
            {
                nozzle_frame_release(FrameForRenderThread);
                return;
            }

            const int32 CopyError = FNozzleNativeBridge::CopyFrameToNativeTexture_RenderThread(FrameForRenderThread, NativeView, RenderThreadDiagnostics);
            nozzle_frame_release(FrameForRenderThread);
            StoreReceiverRenderDiagnostics(LocalRenderState, RenderThreadDiagnostics);
            if(CopyError != 0)
            {
                UE_LOG(LogNozzle, Error, TEXT("native frame copy failed: %s"), *RenderThreadDiagnostics.Message);
            }
        }
    );

    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Width = static_cast<int32>(FrameInfo.width);
    LastDiagnostics.Height = static_cast<int32>(FrameInfo.height);
    LastDiagnostics.Message = TEXT("queued platform frame copy to Unreal render target on the render thread; render-thread result is drained on tick/diagnostics query");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: PollFrame cannot acquire nozzle frames");
    return false;
#endif
}

bool UNozzleReceiverComponent::IsReceiverRunning() const
{
    return bReceiverRunning;
}

FNozzleRuntimeDiagnostics UNozzleReceiverComponent::GetLastDiagnostics()
{
    DrainRenderThreadDiagnostics();
    return LastDiagnostics;
}

FNozzleRuntimeDiagnostics UNozzleReceiverComponent::GetLastRenderDiagnostics()
{
    DrainRenderThreadDiagnostics();
    return LastRenderDiagnostics;
}

int64 UNozzleReceiverComponent::GetLastRenderSequence()
{
    DrainRenderThreadDiagnostics();
    return LastRenderSequence;
}
