#pragma once

#include "CoreMinimal.h"
#include "NozzleDiagnostics.h"

class FTextureRenderTargetResource;

struct FNozzleNativeTextureView
{
    void* NativeTexture = nullptr;
    int32 Width = 0;
    int32 Height = 0;
};

class FNozzleNativeBridge final
{
public:
    static const TCHAR* UnsupportedRHIMessage();
    static FString GetSelectedRHIName();
    static bool IsD3D11RHI(FString* OutRHIName = nullptr);
    static bool IsMetalRHI(FString* OutRHIName = nullptr);
    static FNozzleRuntimeDiagnostics MakeRuntimeDiagnostics();
    static bool CaptureNativeTexture_RenderThread(FTextureRenderTargetResource* RenderTargetResource, FNozzleNativeTextureView& OutView, FNozzleRuntimeDiagnostics& OutDiagnostics);
};
