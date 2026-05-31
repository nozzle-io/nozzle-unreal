#include "NozzleSenderComponent.h"

#include "Engine/TextureRenderTarget2D.h"
#include "NozzleD3D11Bridge.h"
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
    LastDiagnostics = FNozzleD3D11Bridge::MakeRuntimeDiagnostics();
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
    FTCHARToUTF8 SenderNameUtf8(*SenderName);
    NozzleSenderDesc Desc{};
    Desc.name = SenderNameUtf8.Get();
    Desc.application_name = "Unreal";
    Desc.ring_buffer_size = 3;
    Desc.allow_format_fallback = 0;
    Desc.fallback_flags = NOZZLE_FALLBACK_NONE;
    Desc.fallback_flags_valid = 1;

    const NozzleErrorCode Error = nozzle_sender_create(&Desc, &SenderHandle);
    if(Error != NOZZLE_OK || SenderHandle == nullptr)
    {
        SenderHandle = nullptr;
        bSenderRunning = false;
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = FString::Printf(TEXT("nozzle_sender_create failed with error code %d"), static_cast<int32>(Error));
        UE_LOG(LogNozzle, Error, TEXT("%s"), *LastDiagnostics.Message);
        return false;
    }

    bSenderRunning = true;
    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Message = TEXT("nozzle sender created for D3D11 runtime path");
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
        LastDiagnostics = FNozzleD3D11Bridge::MakeRuntimeDiagnostics();
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
    NozzleSender* LocalSenderHandle = SenderHandle;
    FTextureRenderTargetResource* RenderTargetResource = SourceRenderTarget->GameThread_GetRenderTargetResource();
    if(LocalSenderHandle == nullptr || RenderTargetResource == nullptr)
    {
        LastDiagnostics.State = ENozzleRuntimeState::Error;
        LastDiagnostics.bCanUseRuntime = false;
        LastDiagnostics.Message = TEXT("PublishFrame blocked: sender handle or render target resource is null");
        return false;
    }

    ENQUEUE_RENDER_COMMAND(NozzleUnrealPublishD3D11)(
        [LocalSenderHandle, RenderTargetResource](FRHICommandListImmediate& RHICmdList)
        {
            FNozzleNativeTextureView NativeView;
            FNozzleRuntimeDiagnostics RenderThreadDiagnostics;
            if(!FNozzleD3D11Bridge::CaptureNativeTexture_RenderThread(RenderTargetResource, NativeView, RenderThreadDiagnostics))
            {
                UE_LOG(LogNozzle, Warning, TEXT("D3D11 sender publish skipped: %s"), *RenderThreadDiagnostics.Message);
                return;
            }

            const NozzleErrorCode Error = nozzle_sender_publish_native_texture_ex(
                LocalSenderHandle,
                NativeView.NativeTexture,
                static_cast<uint32>(NativeView.Width),
                static_cast<uint32>(NativeView.Height),
                NOZZLE_FORMAT_BGRA8_UNORM,
                NOZZLE_FORMAT_BGRA8_UNORM
            );
            if(Error != NOZZLE_OK)
            {
                UE_LOG(LogNozzle, Error, TEXT("nozzle_sender_publish_native_texture_ex failed with error code %d"), static_cast<int32>(Error));
            }
        }
    );

    LastDiagnostics.State = ENozzleRuntimeState::Running;
    LastDiagnostics.Width = SourceRenderTarget->SizeX;
    LastDiagnostics.Height = SourceRenderTarget->SizeY;
    LastDiagnostics.Message = TEXT("queued D3D11 native texture publish on the render thread; CI remains static until Unreal Engine executes this path");
    return true;
#else
    LastDiagnostics.State = ENozzleRuntimeState::Unavailable;
    LastDiagnostics.bCanUseRuntime = false;
    LastDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: PublishFrame cannot call nozzle_sender_publish_native_texture_ex");
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
