#include "NozzleD3D11Bridge.h"

#include "DynamicRHI.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "TextureResource.h"

const TCHAR* FNozzleD3D11Bridge::SupportedBackendName()
{
    return TEXT("D3D11");
}

const TCHAR* FNozzleD3D11Bridge::UnsupportedRHIMessage()
{
    return TEXT("unsupported RHI: nozzle-unreal runtime currently supports Win64 D3D11 only; D3D12, macOS, and Linux are not supported");
}

FString FNozzleD3D11Bridge::GetSelectedRHIName()
{
    if(GDynamicRHI == nullptr)
    {
        return TEXT("Unknown");
    }

    const TCHAR* RHIName = GDynamicRHI->GetName();
    return RHIName != nullptr ? FString(RHIName) : FString(TEXT("Unknown"));
}

bool FNozzleD3D11Bridge::IsD3D11RHI(FString* OutRHIName)
{
    const FString RHIName = GetSelectedRHIName();
    if(OutRHIName != nullptr)
    {
        *OutRHIName = RHIName;
    }

    return RHIName.Contains(TEXT("D3D11"), ESearchCase::IgnoreCase);
}

FNozzleRuntimeDiagnostics FNozzleD3D11Bridge::MakeRuntimeDiagnostics()
{
    FNozzleRuntimeDiagnostics Diagnostics;
    Diagnostics.bWithNozzleCore = WITH_NOZZLE_CORE != 0;
    Diagnostics.bSupportedPlatform = NOZZLE_UNREAL_TARGET_WIN64 != 0;
    Diagnostics.SelectedRHI = GetSelectedRHIName();
    Diagnostics.bD3D11RHI = IsD3D11RHI();
    Diagnostics.Backend = Diagnostics.bD3D11RHI ? SupportedBackendName() : TEXT("unsupported");
    Diagnostics.Format = TEXT("BGRA8_UNORM intended; native Unreal render-target format still must be measured in engine smoke tests");
    Diagnostics.TransferMode = TEXT("D3D11 native texture path; runtime smoke not yet proven");

    if(!Diagnostics.bSupportedPlatform)
    {
        Diagnostics.State = ENozzleRuntimeState::UnsupportedRHI;
        Diagnostics.Message = TEXT("unsupported platform: only Win64 D3D11 is wired for the first nozzle-unreal runtime path");
    }
    else if(!Diagnostics.bD3D11RHI)
    {
        Diagnostics.State = ENozzleRuntimeState::UnsupportedRHI;
        Diagnostics.Message = UnsupportedRHIMessage();
    }
    else if(!Diagnostics.bWithNozzleCore)
    {
        Diagnostics.State = ENozzleRuntimeState::Unavailable;
        Diagnostics.Message = TEXT("WITH_NOZZLE_CORE=0: staged nozzle headers/library/runtime DLL are missing, so runtime calls are disabled");
    }
    else
    {
        Diagnostics.State = ENozzleRuntimeState::Ready;
        Diagnostics.bCanUseRuntime = true;
        Diagnostics.Message = TEXT("Win64 D3D11 RHI detected and WITH_NOZZLE_CORE=1; runtime API may attempt D3D11 texture sharing, but CI is still static unless Unreal Engine is present");
    }

    return Diagnostics;
}

bool FNozzleD3D11Bridge::CaptureNativeTexture_RenderThread(FTextureRenderTargetResource* RenderTargetResource, FNozzleNativeTextureView& OutView, FNozzleRuntimeDiagnostics& OutDiagnostics)
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
        OutDiagnostics.Message = TEXT("FRHITexture::GetNativeResource returned null under D3D11");
        return false;
    }

    OutDiagnostics.State = ENozzleRuntimeState::Ready;
    OutDiagnostics.Message = TEXT("captured D3D11 native texture pointer from FRHITexture::GetNativeResource on the render thread");
    return true;
}
