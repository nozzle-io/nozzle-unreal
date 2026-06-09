#include "NozzleNativeBridge.h"

#if NOZZLE_UNREAL_TARGET_MAC
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>

bool NozzleUnrealExtractMetalDeviceFromNativeTexture(void* NativeTexture, void*& OutDevice)
{
    OutDevice = nullptr;
    if(NativeTexture == nullptr)
    {
        return false;
    }

    id<MTLTexture> Texture = (__bridge id<MTLTexture>)NativeTexture;
    if(Texture == nil)
    {
        return false;
    }

    id<MTLDevice> Device = [Texture device];
    if(Device == nil)
    {
        return false;
    }

    OutDevice = (__bridge void*)Device;
    return true;
}
#endif


FNozzleMetalTextureDiagnostics NozzleUnrealDescribeMetalTexture(void* NativeTexture)
{
    FNozzleMetalTextureDiagnostics Diagnostics;
    if(NativeTexture == nullptr)
    {
        Diagnostics.Message = TEXT("native Metal texture pointer is null");
        return Diagnostics;
    }

    @autoreleasepool
    {
        id<MTLTexture> Texture = (__bridge id<MTLTexture>)NativeTexture;
        if(Texture == nil)
        {
            Diagnostics.Message = TEXT("native Metal texture bridge produced nil id<MTLTexture>");
            return Diagnostics;
        }

        Diagnostics.bValidTexture = true;
        Diagnostics.Width = static_cast<int32>(Texture.width);
        Diagnostics.Height = static_cast<int32>(Texture.height);

        IOSurfaceRef Surface = Texture.iosurface;
        if(Surface != nullptr)
        {
            Diagnostics.bIOSurfaceBacked = true;
            Diagnostics.IOSurfaceID = static_cast<int64>(IOSurfaceGetID(Surface));
        }

        Diagnostics.Details = FString::Printf(
            TEXT("source=FRHITexture::GetNativeResource MTLPixelFormat=%llu width=%d height=%d storageMode=%llu usage=0x%llx iosurface_backed=%s iosurface_id=%lld"),
            static_cast<unsigned long long>(Texture.pixelFormat),
            Diagnostics.Width,
            Diagnostics.Height,
            static_cast<unsigned long long>(Texture.storageMode),
            static_cast<unsigned long long>(Texture.usage),
            Diagnostics.bIOSurfaceBacked ? TEXT("true") : TEXT("false"),
            static_cast<long long>(Diagnostics.IOSurfaceID));

        Diagnostics.Message = Diagnostics.bIOSurfaceBacked
            ? TEXT("native Metal texture is IOSurface-backed")
            : TEXT("native Metal texture is not IOSurface-backed; direct cross-process Metal publish cannot pass");
    }

    return Diagnostics;
}
