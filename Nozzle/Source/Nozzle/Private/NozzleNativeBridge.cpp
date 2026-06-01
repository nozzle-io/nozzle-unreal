#include "NozzleNativeBridge.h"

#include "DynamicRHI.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "TextureResource.h"

#ifndef NOZZLE_UNREAL_TARGET_WIN64
#define NOZZLE_UNREAL_TARGET_WIN64 0
#endif

#ifndef NOZZLE_UNREAL_TARGET_MAC
#define NOZZLE_UNREAL_TARGET_MAC 0
#endif

#ifndef NOZZLE_UNREAL_D3D11_RUNTIME
#define NOZZLE_UNREAL_D3D11_RUNTIME 0
#endif

#ifndef NOZZLE_UNREAL_METAL_RUNTIME
#define NOZZLE_UNREAL_METAL_RUNTIME 0
#endif

#if WITH_NOZZLE_CORE
#include "Containers/StringConv.h"
#define NOZZLE_UNREAL_NATIVE_TARGET_WIN64 NOZZLE_UNREAL_TARGET_WIN64
#define NOZZLE_UNREAL_NATIVE_TARGET_MACOS NOZZLE_UNREAL_TARGET_MAC
#define NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME NOZZLE_UNREAL_D3D11_RUNTIME
#define NOZZLE_UNREAL_NATIVE_METAL_RUNTIME NOZZLE_UNREAL_METAL_RUNTIME
#define NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE WITH_NOZZLE_CORE
#include "nozzle_unreal_native_bridge.h"
#endif

#if NOZZLE_UNREAL_TARGET_WIN64
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if NOZZLE_UNREAL_TARGET_MAC
extern bool NozzleUnrealExtractMetalDeviceFromNativeTexture(void* NativeTexture, void*& OutDevice);
#else
static bool NozzleUnrealExtractMetalDeviceFromNativeTexture(void* NativeTexture, void*& OutDevice)
{
    OutDevice = nullptr;
    return false;
}
#endif

namespace
{

void ApplyError(FNozzleRuntimeDiagnostics& Diagnostics, const FString& Message)
{
    Diagnostics.State = ENozzleRuntimeState::Error;
    Diagnostics.bCanUseRuntime = false;
    Diagnostics.Message = Message;
}

} // namespace

const TCHAR* FNozzleNativeBridge::UnsupportedRHIMessage()
{
    return TEXT("unsupported RHI: nozzle-unreal runtime source currently wires Win64 D3D11 and macOS Metal only; D3D12 and Linux are not supported");
}

FString FNozzleNativeBridge::GetSelectedRHIName()
{
    if(GDynamicRHI == nullptr)
    {
        return TEXT("Unknown");
    }

    const TCHAR* RHIName = GDynamicRHI->GetName();
    return RHIName != nullptr ? FString(RHIName) : FString(TEXT("Unknown"));
}

bool FNozzleNativeBridge::IsD3D11RHI(FString* OutRHIName)
{
    const FString RHIName = GetSelectedRHIName();
    if(OutRHIName != nullptr)
    {
        *OutRHIName = RHIName;
    }

    return RHIName.Contains(TEXT("D3D11"), ESearchCase::IgnoreCase);
}

bool FNozzleNativeBridge::IsMetalRHI(FString* OutRHIName)
{
    const FString RHIName = GetSelectedRHIName();
    if(OutRHIName != nullptr)
    {
        *OutRHIName = RHIName;
    }

    return RHIName.Contains(TEXT("Metal"), ESearchCase::IgnoreCase);
}

FNozzleRuntimeDiagnostics FNozzleNativeBridge::MakeRuntimeDiagnostics()
{
    FNozzleRuntimeDiagnostics Diagnostics;
    Diagnostics.bWithNozzleCore = WITH_NOZZLE_CORE != 0;
    Diagnostics.bSupportedPlatform = (NOZZLE_UNREAL_TARGET_WIN64 != 0) || (NOZZLE_UNREAL_TARGET_MAC != 0);
    Diagnostics.SelectedRHI = GetSelectedRHIName();
    Diagnostics.bD3D11RHI = IsD3D11RHI();
    Diagnostics.bMetalRHI = IsMetalRHI();
    Diagnostics.Format = TEXT("BGRA8_UNORM intended first; native Unreal render-target format still must be measured in engine smoke tests");

    const bool bCanUseD3D11 = NOZZLE_UNREAL_TARGET_WIN64 != 0 && NOZZLE_UNREAL_D3D11_RUNTIME != 0 && Diagnostics.bD3D11RHI;
    const bool bCanUseMetal = NOZZLE_UNREAL_TARGET_MAC != 0 && NOZZLE_UNREAL_METAL_RUNTIME != 0 && Diagnostics.bMetalRHI;

    if(bCanUseD3D11)
    {
        Diagnostics.Backend = TEXT("D3D11");
        Diagnostics.TransferMode = TEXT("D3D11 native texture path using Unreal's native device; runtime smoke not yet proven");
    }
    else if(bCanUseMetal)
    {
        Diagnostics.Backend = TEXT("Metal");
        Diagnostics.TransferMode = TEXT("Metal native texture path using Unreal's native device; IOSurface backing, synchronization, and runtime smoke are not yet proven");
    }
    else
    {
        Diagnostics.Backend = TEXT("unsupported");
        Diagnostics.TransferMode = TEXT("unsupported RHI/platform path");
    }

    if(!Diagnostics.bSupportedPlatform)
    {
        Diagnostics.State = ENozzleRuntimeState::UnsupportedRHI;
        Diagnostics.Message = TEXT("unsupported platform: only Win64 D3D11 and macOS Metal source paths are wired in this phase");
    }
    else if(!bCanUseD3D11 && !bCanUseMetal)
    {
        Diagnostics.State = ENozzleRuntimeState::UnsupportedRHI;
        Diagnostics.Message = UnsupportedRHIMessage();
    }
    else if(!Diagnostics.bWithNozzleCore)
    {
        Diagnostics.State = ENozzleRuntimeState::Unavailable;
        Diagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: staged nozzle headers/library/runtime binary are missing, so runtime calls are disabled");
    }
    else
    {
        Diagnostics.State = ENozzleRuntimeState::Ready;
        Diagnostics.bCanUseRuntime = true;
        Diagnostics.Message = TEXT("platform RHI detected and WITH_NOZZLE_CORE=1; runtime API may attempt native texture sharing through Unreal's native device, but CI is still static unless Unreal Engine is present");
    }

    return Diagnostics;
}

bool FNozzleNativeBridge::CaptureNativeTexture_RenderThread(FTextureRenderTargetResource* RenderTargetResource, FNozzleNativeTextureView& OutView, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDiagnostics = MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return false;
    }

    if(RenderTargetResource == nullptr)
    {
        ApplyError(OutDiagnostics, TEXT("render target resource is null"));
        return false;
    }

    FTextureRHIRef TextureRHI = RenderTargetResource->GetRenderTargetTexture();
    if(!TextureRHI.IsValid())
    {
        ApplyError(OutDiagnostics, TEXT("render target has no RHI texture"));
        return false;
    }

    OutView.NativeTexture = TextureRHI->GetNativeResource();
    const FIntPoint Size = RenderTargetResource->GetSizeXY();
    OutView.Width = Size.X;
    OutView.Height = Size.Y;

    OutDiagnostics.Width = OutView.Width;
    OutDiagnostics.Height = OutView.Height;
    if(OutView.NativeTexture == nullptr)
    {
        ApplyError(OutDiagnostics, TEXT("FRHITexture::GetNativeResource returned null under the selected native RHI"));
        return false;
    }

    OutDiagnostics.State = ENozzleRuntimeState::Ready;
    OutDiagnostics.Message = TEXT("captured native texture pointer from FRHITexture::GetNativeResource on the render thread");
    return true;
}

bool FNozzleNativeBridge::CaptureNativeTextureAndDevice_RenderThread(FTextureRenderTargetResource* RenderTargetResource, FNozzleNativeTextureView& OutTextureView, FNozzleNativeDeviceView& OutDeviceView, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDeviceView = FNozzleNativeDeviceView{};
    if(!CaptureNativeTexture_RenderThread(RenderTargetResource, OutTextureView, OutDiagnostics))
    {
        return false;
    }

    if(OutDiagnostics.bD3D11RHI)
    {
#if NOZZLE_UNREAL_TARGET_WIN64
        ID3D11Texture2D* D3D11Texture = static_cast<ID3D11Texture2D*>(OutTextureView.NativeTexture);
        if(D3D11Texture == nullptr)
        {
            ApplyError(OutDiagnostics, TEXT("D3D11 RHI selected but native texture is null"));
            return false;
        }

        ID3D11Device* D3D11Device = nullptr;
        D3D11Texture->GetDevice(&D3D11Device);
        if(D3D11Device == nullptr)
        {
            ApplyError(OutDiagnostics, TEXT("ID3D11Texture2D::GetDevice returned null"));
            return false;
        }

        ID3D11DeviceContext* D3D11Context = nullptr;
        D3D11Device->GetImmediateContext(&D3D11Context);
        if(D3D11Context == nullptr)
        {
            D3D11Device->Release();
            ApplyError(OutDiagnostics, TEXT("ID3D11Device::GetImmediateContext returned null"));
            return false;
        }

        OutDeviceView.Device = D3D11Device;
        OutDeviceView.Context = D3D11Context;
        D3D11Context->Release();
        D3D11Device->Release();
        OutDiagnostics.Message = TEXT("captured Unreal D3D11 texture, device, and immediate context on the render thread");
        return true;
#else
        ApplyError(OutDiagnostics, TEXT("D3D11 RHI selected on a non-Win64 build"));
        return false;
#endif
    }

    if(OutDiagnostics.bMetalRHI)
    {
        void* MetalDevice = nullptr;
        if(!NozzleUnrealExtractMetalDeviceFromNativeTexture(OutTextureView.NativeTexture, MetalDevice) || MetalDevice == nullptr)
        {
            ApplyError(OutDiagnostics, TEXT("Metal RHI selected but native MTLTexture device extraction failed"));
            return false;
        }

        OutDeviceView.Device = MetalDevice;
        OutDiagnostics.Message = TEXT("captured Unreal Metal texture and device on the render thread; IOSurface backing and synchronization still need smoke proof");
        return true;
    }

    ApplyError(OutDiagnostics, UnsupportedRHIMessage());
    return false;
}

int32 FNozzleNativeBridge::CreateSenderForNativeDevice_RenderThread(const FString& SenderName, const FNozzleNativeDeviceView& DeviceView, NozzleSender*& InOutSender, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDiagnostics = MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return static_cast<int32>(-1);
    }

#if WITH_NOZZLE_CORE
    if(InOutSender != nullptr)
    {
        return static_cast<int32>(NOZZLE_OK);
    }

    FTCHARToUTF8 SenderNameUtf8(*SenderName);
    const NozzleSenderDesc Desc = nozzle_unreal_native::make_sender_desc(SenderNameUtf8.Get(), "Unreal", 3);
    NozzleErrorCode Error = NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    if(OutDiagnostics.bD3D11RHI)
    {
        Error = nozzle_unreal_native::create_d3d11_sender(&Desc, {DeviceView.Device, DeviceView.Context}, true, &InOutSender);
    }
    else if(OutDiagnostics.bMetalRHI)
    {
        Error = nozzle_unreal_native::create_metal_sender(&Desc, {DeviceView.Device}, true, &InOutSender);
    }

    if(Error != NOZZLE_OK || InOutSender == nullptr)
    {
        InOutSender = nullptr;
        ApplyError(OutDiagnostics, FString::Printf(TEXT("nozzle_sender_create_with_native_device failed with error code %d"), static_cast<int32>(Error)));
        return static_cast<int32>(Error);
    }

    OutDiagnostics.State = ENozzleRuntimeState::Running;
    OutDiagnostics.Message = TEXT("created nozzle sender with Unreal's native device on the render thread");
    return static_cast<int32>(NOZZLE_OK);
#else
    OutDiagnostics.State = ENozzleRuntimeState::Unavailable;
    OutDiagnostics.bCanUseRuntime = false;
    OutDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: native-device sender creation is disabled");
    return static_cast<int32>(-1);
#endif
}

int32 FNozzleNativeBridge::CreateReceiverForBackend(const FString& SenderName, NozzleReceiver*& OutReceiver, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDiagnostics = MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return static_cast<int32>(-1);
    }

#if WITH_NOZZLE_CORE
    FTCHARToUTF8 SenderNameUtf8(*SenderName);
    const NozzleReceiverDesc Desc = nozzle_unreal_native::make_receiver_desc(SenderNameUtf8.Get(), "Unreal");
    const NozzleErrorCode Error = nozzle_unreal_native::create_receiver_for_backend(&Desc, OutDiagnostics.bD3D11RHI, OutDiagnostics.bMetalRHI, &OutReceiver);
    if(Error != NOZZLE_OK || OutReceiver == nullptr)
    {
        OutReceiver = nullptr;
        ApplyError(OutDiagnostics, FString::Printf(TEXT("nozzle_receiver_create failed with error code %d"), static_cast<int32>(Error)));
        return static_cast<int32>(Error);
    }

    OutDiagnostics.State = ENozzleRuntimeState::Running;
    OutDiagnostics.Message = TEXT("created nozzle receiver through the shared native bridge backend gate");
    return static_cast<int32>(NOZZLE_OK);
#else
    OutDiagnostics.State = ENozzleRuntimeState::Unavailable;
    OutDiagnostics.bCanUseRuntime = false;
    OutDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: receiver creation is disabled");
    return static_cast<int32>(-1);
#endif
}

int32 FNozzleNativeBridge::PublishNativeTexture_RenderThread(NozzleSender* Sender, const FNozzleNativeTextureView& TextureView, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDiagnostics = MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return static_cast<int32>(-1);
    }

#if WITH_NOZZLE_CORE
    NozzleErrorCode Error = NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    if(OutDiagnostics.bD3D11RHI)
    {
        Error = nozzle_unreal_native::publish_d3d11_texture(Sender, {TextureView.NativeTexture, static_cast<uint32>(TextureView.Width), static_cast<uint32>(TextureView.Height), NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM});
    }
    else if(OutDiagnostics.bMetalRHI)
    {
        Error = nozzle_unreal_native::publish_metal_texture(Sender, {TextureView.NativeTexture, static_cast<uint32>(TextureView.Width), static_cast<uint32>(TextureView.Height), NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM});
    }

    if(Error != NOZZLE_OK)
    {
        ApplyError(OutDiagnostics, FString::Printf(TEXT("nozzle native texture publish failed with error code %d"), static_cast<int32>(Error)));
        return static_cast<int32>(Error);
    }

    OutDiagnostics.State = ENozzleRuntimeState::Running;
    OutDiagnostics.Width = TextureView.Width;
    OutDiagnostics.Height = TextureView.Height;
    OutDiagnostics.Message = TEXT("published Unreal native texture through the shared native bridge seam");
    return static_cast<int32>(NOZZLE_OK);
#else
    OutDiagnostics.State = ENozzleRuntimeState::Unavailable;
    OutDiagnostics.bCanUseRuntime = false;
    OutDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: native texture publish is disabled");
    return static_cast<int32>(-1);
#endif
}

int32 FNozzleNativeBridge::CopyFrameToNativeTexture_RenderThread(NozzleFrame* Frame, const FNozzleNativeTextureView& TextureView, FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutDiagnostics = MakeRuntimeDiagnostics();
    if(!OutDiagnostics.bCanUseRuntime)
    {
        return static_cast<int32>(-1);
    }

#if WITH_NOZZLE_CORE
    NozzleErrorCode Error = NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    if(OutDiagnostics.bD3D11RHI)
    {
        Error = nozzle_unreal_native::copy_frame_to_d3d11_texture(Frame, {TextureView.NativeTexture, static_cast<uint32>(TextureView.Width), static_cast<uint32>(TextureView.Height), NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM});
    }
    else if(OutDiagnostics.bMetalRHI)
    {
        Error = nozzle_unreal_native::copy_frame_to_metal_texture(Frame, {TextureView.NativeTexture, static_cast<uint32>(TextureView.Width), static_cast<uint32>(TextureView.Height), NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM});
    }

    if(Error != NOZZLE_OK)
    {
        ApplyError(OutDiagnostics, FString::Printf(TEXT("nozzle frame copy to native texture failed with error code %d"), static_cast<int32>(Error)));
        return static_cast<int32>(Error);
    }

    OutDiagnostics.State = ENozzleRuntimeState::Running;
    OutDiagnostics.Width = TextureView.Width;
    OutDiagnostics.Height = TextureView.Height;
    OutDiagnostics.Message = TEXT("copied nozzle frame to Unreal native texture through the shared native bridge seam");
    return static_cast<int32>(NOZZLE_OK);
#else
    OutDiagnostics.State = ENozzleRuntimeState::Unavailable;
    OutDiagnostics.bCanUseRuntime = false;
    OutDiagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: native texture copy is disabled");
    return static_cast<int32>(-1);
#endif
}
