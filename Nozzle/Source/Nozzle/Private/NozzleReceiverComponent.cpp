#include "NozzleReceiverComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "NozzleNativeBridge.h"
#include "NozzleRuntimeModule.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#if WITH_NOZZLE_CORE
#include "Containers/StringConv.h"
#include "nozzle/nozzle_c.h"
#endif

UNozzleReceiverComponent::UNozzleReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNozzleReceiverComponent::BeginPlay()
{
    Super::BeginPlay();

    if(bAutoStart)
    {
        StartReceiver();
    }
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
    FTCHARToUTF8 SenderNameUtf8(*SenderName);
    NozzleReceiverDesc Desc{};
    Desc.name = SenderNameUtf8.Get();
    Desc.application_name = "Unreal";
    Desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;

    const NozzleErrorCode Error = nozzle_receiver_create(&Desc, &ReceiverHandle);
    if(Error != NOZZLE_OK || ReceiverHandle == nullptr)
    {
        ReceiverHandle = nullptr;
        bReceiverRunning = false;
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = FString::Printf(TEXT("nozzle_receiver_create failed with error code %d"), static_cast<int32>(Error));
        UE_LOG(LogNozzle, Error, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }

    bReceiverRunning = true;
    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Message = TEXT("nozzle receiver created for platform native runtime path");
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
    if(ReceiverHandle != nullptr)
    {
        FlushRenderingCommands();
        nozzle_receiver_destroy(ReceiverHandle);
        ReceiverHandle = nullptr;
    }
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
    ENQUEUE_RENDER_COMMAND(NozzleUnrealCopyD3D11Frame)(
        [FrameForRenderThread, RenderTargetResource, Width = FrameInfo.width, Height = FrameInfo.height](FRHICommandListImmediate& RHICmdList)
        {
            FNozzleNativeTextureView NativeView;
            FNozzleRuntimeDiagnostics RenderThreadDiagnostics;
            if(!FNozzleNativeBridge::CaptureNativeTexture_RenderThread(RenderTargetResource, NativeView, RenderThreadDiagnostics))
            {
                UE_LOG(LogNozzle, Warning, TEXT("platform native receiver copy skipped: %s"), *RenderThreadDiagnostics.Message);
                nozzle_frame_release(FrameForRenderThread);
                return;
            }

            const NozzleErrorCode CopyError = nozzle_frame_copy_to_native_texture(
                FrameForRenderThread,
                NativeView.NativeTexture,
                Width,
                Height,
                NOZZLE_FORMAT_BGRA8_UNORM
            );
            nozzle_frame_release(FrameForRenderThread);
            if(CopyError != NOZZLE_OK)
            {
                UE_LOG(LogNozzle, Error, TEXT("nozzle_frame_copy_to_native_texture failed with error code %d"), static_cast<int32>(CopyError));
            }
        }
    );

    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Width = static_cast<int32>(FrameInfo.width);
    LastDiagnostics.Height = static_cast<int32>(FrameInfo.height);
    LastDiagnostics.Message = TEXT("queued platform frame copy to Unreal render target on the render thread; CI remains static until Unreal Engine executes this path");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: PollFrame cannot call nozzle_receiver_acquire_frame");
    return false;
#endif
}

bool UNozzleReceiverComponent::IsReceiverRunning() const
{
    return bReceiverRunning;
}

FNozzleRuntimeDiagnostics UNozzleReceiverComponent::GetLastDiagnostics() const
{
    return LastDiagnostics;
}
