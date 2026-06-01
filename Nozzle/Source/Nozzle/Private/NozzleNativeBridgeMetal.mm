#include "NozzleNativeBridge.h"

#if NOZZLE_UNREAL_TARGET_MAC
#import <Metal/Metal.h>

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
