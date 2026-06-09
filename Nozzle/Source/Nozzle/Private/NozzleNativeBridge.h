#pragma once

#include "CoreMinimal.h"
#include "NozzleDiagnostics.h"

class FTextureRenderTargetResource;
struct NozzleFrame;
struct NozzleReceiver;
struct NozzleSender;

struct FNozzleNativeTextureView
{
    void* NativeTexture = nullptr;
    int32 Width = 0;
    int32 Height = 0;
};

struct FNozzleNativeDeviceView
{
    void* Device = nullptr;
    void* Context = nullptr;
};

struct FNozzleMetalTextureDiagnostics
{
    bool bValidTexture = false;
    bool bIOSurfaceBacked = false;
    int64 IOSurfaceID = 0;
    int32 Width = 0;
    int32 Height = 0;
    FString Details;
    FString Message;
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
    static bool CaptureNativeTextureAndDevice_RenderThread(FTextureRenderTargetResource* RenderTargetResource, FNozzleNativeTextureView& OutTextureView, FNozzleNativeDeviceView& OutDeviceView, FNozzleRuntimeDiagnostics& OutDiagnostics);
    static int32 CreateSenderForNativeDevice_RenderThread(const FString& SenderName, const FNozzleNativeDeviceView& DeviceView, NozzleSender*& InOutSender, FNozzleRuntimeDiagnostics& OutDiagnostics);
    static int32 CreateReceiverForBackend(const FString& SenderName, NozzleReceiver*& OutReceiver, FNozzleRuntimeDiagnostics& OutDiagnostics);
    static int32 PublishNativeTexture_RenderThread(NozzleSender* Sender, const FNozzleNativeTextureView& TextureView, FNozzleRuntimeDiagnostics& OutDiagnostics);
    static int32 CopyFrameToNativeTexture_RenderThread(NozzleFrame* Frame, const FNozzleNativeTextureView& TextureView, FNozzleRuntimeDiagnostics& OutDiagnostics);
};
