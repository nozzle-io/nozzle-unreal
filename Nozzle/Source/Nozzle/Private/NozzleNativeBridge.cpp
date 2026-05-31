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
        Diagnostics.TransferMode = TEXT("D3D11 native texture path; runtime smoke not yet proven");
    }
    else if(bCanUseMetal)
    {
        Diagnostics.Backend = TEXT("Metal");
        Diagnostics.TransferMode = TEXT("Metal native texture path; IOSurface backing, synchronization, and runtime smoke are not yet proven");
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
        Diagnostics.Message = TEXT("platform RHI detected and WITH_NOZZLE_CORE=1; runtime API may attempt native texture sharing, but CI is still static unless Unreal Engine is present");
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
        OutDiagnostics.State = ENozzleRuntimeState::Error;
        OutDiagnostics.bCanUseRuntime = false;
        OutDiagnostics.Message = TEXT("render target resource is null");
        return false;
    }

    FTextureRHIRef TextureRHI = RenderTargetResource->GetRenderTargetTexture();
    if(!TextureRHI.IsValid())
    {
        OutDiagnostics.State = ENozzleRuntimeState::Error;
        OutDiagnostics.bCanUseRuntime = false;
        OutDiagnostics.Message = TEXT("render target has no RHI texture");
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
        OutDiagnostics.State = ENozzleRuntimeState::Error;
        OutDiagnostics.bCanUseRuntime = false;
        OutDiagnostics.Message = TEXT("FRHITexture::GetNativeResource returned null under the selected native RHI");
        return false;
    }

    OutDiagnostics.State = ENozzleRuntimeState::Ready;
    OutDiagnostics.Message = TEXT("captured native texture pointer from FRHITexture::GetNativeResource on the render thread");
    return true;
}
