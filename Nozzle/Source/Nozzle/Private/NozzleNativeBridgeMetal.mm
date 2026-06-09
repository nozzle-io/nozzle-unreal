#include "NozzleNativeBridge.h"

#if NOZZLE_UNREAL_TARGET_MAC
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>

namespace
{

constexpr int32 NozzleUnrealIOSurfaceAlignmentBytes = 64;

bool NozzleUnrealComputeAlignedBytesPerRow(int32 Width, uint32& OutBytesPerRow)
{
    OutBytesPerRow = 0u;
    constexpr uint32 BytesPerElement = 4u;
    const uint32 Alignment = static_cast<uint32>(NozzleUnrealIOSurfaceAlignmentBytes);
    if(Width <= 0 || static_cast<uint64>(Width) > (static_cast<uint64>(MAX_uint32) / BytesPerElement))
    {
        return false;
    }

    const uint64 UnalignedBytesPerRow = static_cast<uint64>(Width) * BytesPerElement;
    const uint64 AlignedBytesPerRow = (UnalignedBytesPerRow + Alignment - 1u) & ~(static_cast<uint64>(Alignment) - 1u);
    if(AlignedBytesPerRow > MAX_uint32)
    {
        return false;
    }

    OutBytesPerRow = static_cast<uint32>(AlignedBytesPerRow);
    return true;
}

void NozzleUnrealClearMetalIntermediateCache(FNozzleMetalIntermediateTextureCache& Cache)
{
    if(Cache.Texture != nullptr)
    {
        CFRelease(Cache.Texture);
    }
    if(Cache.Surface != nullptr)
    {
        CFRelease(Cache.Surface);
    }
    if(Cache.CommandQueue != nullptr)
    {
        CFRelease(Cache.CommandQueue);
    }
    Cache = FNozzleMetalIntermediateTextureCache{};
}

bool NozzleUnrealEnsureMetalIntermediateTexture(
    id<MTLDevice> Device,
    int32 Width,
    int32 Height,
    FNozzleMetalIntermediateTextureCache& Cache,
    FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    if(Device == nil)
    {
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate creation failed: device is nil");
        return false;
    }
    if(Width <= 0 || Height <= 0)
    {
        OutDiagnostics.Message = FString::Printf(TEXT("Metal IOSurface intermediate creation failed: invalid size %dx%d"), Width, Height);
        return false;
    }

    constexpr uint64 DesiredPixelFormat = static_cast<uint64>(MTLPixelFormatBGRA8Unorm);
    if(Cache.Texture != nullptr && Cache.Surface != nullptr && Cache.CommandQueue != nullptr && Cache.Device == (__bridge void*)Device && Cache.Width == Width && Cache.Height == Height && Cache.PixelFormat == DesiredPixelFormat)
    {
        return true;
    }

    NozzleUnrealClearMetalIntermediateCache(Cache);

    constexpr uint32 BytesPerElement = 4u;
    uint32 BytesPerRow = 0u;
    if(!NozzleUnrealComputeAlignedBytesPerRow(Width, BytesPerRow))
    {
        OutDiagnostics.Message = FString::Printf(TEXT("Metal IOSurface intermediate creation failed: invalid bytes-per-row for width %d"), Width);
        return false;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSDictionary* SurfaceProperties = @{
        (id)kIOSurfaceIsGlobal: @(YES),
        (id)kIOSurfaceWidth: @(Width),
        (id)kIOSurfaceHeight: @(Height),
        (id)kIOSurfacePixelFormat: @(static_cast<OSType>('BGRA')),
        (id)kIOSurfaceBytesPerRow: @(BytesPerRow),
        (id)kIOSurfaceBytesPerElement: @(BytesPerElement),
    };
#pragma clang diagnostic pop

    IOSurfaceRef Surface = IOSurfaceCreate((CFDictionaryRef)SurfaceProperties);
    if(Surface == nullptr)
    {
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate creation failed: IOSurfaceCreate returned null");
        return false;
    }

    MTLTextureDescriptor* TextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:static_cast<NSUInteger>(Width) height:static_cast<NSUInteger>(Height) mipmapped:NO];
    TextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    TextureDescriptor.storageMode = MTLStorageModeShared;

    id<MTLTexture> Texture = [Device newTextureWithDescriptor:TextureDescriptor iosurface:Surface plane:0];
    if(Texture == nil)
    {
        CFRelease(Surface);
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate creation failed: newTextureWithDescriptor:iosurface returned nil");
        return false;
    }

    id<MTLCommandQueue> CommandQueue = [Device newCommandQueue];
    if(CommandQueue == nil)
    {
#if !__has_feature(objc_arc)
        [Texture release];
#endif
        CFRelease(Surface);
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate creation failed: newCommandQueue returned nil");
        return false;
    }

    const IOSurfaceID SurfaceID = IOSurfaceGetID(Surface);
    IOSurfaceRef LookupSurface = SurfaceID != 0 ? IOSurfaceLookup(SurfaceID) : nullptr;
    if(LookupSurface == nullptr)
    {
#if !__has_feature(objc_arc)
        [CommandQueue release];
        [Texture release];
#endif
        CFRelease(Surface);
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate creation failed: IOSurface ID is not globally lookupable");
        return false;
    }
    CFRelease(LookupSurface);

#if __has_feature(objc_arc)
    Cache.Texture = (__bridge_retained void*)Texture;
#else
    Cache.Texture = (void*)Texture;
#endif
    Cache.Surface = Surface;
#if __has_feature(objc_arc)
    Cache.CommandQueue = (__bridge_retained void*)CommandQueue;
#else
    Cache.CommandQueue = (void*)CommandQueue;
#endif
    Cache.Width = Width;
    Cache.Height = Height;
    Cache.Device = (__bridge void*)Device;
    Cache.PixelFormat = DesiredPixelFormat;
    return true;
}

bool NozzleUnrealBlitMetalTextureToIntermediate(
    id<MTLTexture> SourceTexture,
    FNozzleMetalIntermediateTextureCache& Cache,
    FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    id<MTLTexture> DestinationTexture = (__bridge id<MTLTexture>)Cache.Texture;
    id<MTLCommandQueue> CommandQueue = (__bridge id<MTLCommandQueue>)Cache.CommandQueue;
    if(SourceTexture == nil || DestinationTexture == nil || CommandQueue == nil)
    {
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate blit failed: null source, destination, or command queue");
        return false;
    }

    id<MTLCommandBuffer> CommandBuffer = [CommandQueue commandBuffer];
    if(CommandBuffer == nil)
    {
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate blit failed: commandBuffer returned nil");
        return false;
    }

    id<MTLBlitCommandEncoder> BlitEncoder = [CommandBuffer blitCommandEncoder];
    if(BlitEncoder == nil)
    {
        OutDiagnostics.Message = TEXT("Metal IOSurface intermediate blit failed: blitCommandEncoder returned nil");
        return false;
    }

    [BlitEncoder copyFromTexture:SourceTexture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(static_cast<NSUInteger>(Cache.Width), static_cast<NSUInteger>(Cache.Height), 1)
                       toTexture:DestinationTexture
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];

    [BlitEncoder endEncoding];
    [CommandBuffer commit];
    [CommandBuffer waitUntilCompleted];

    if(CommandBuffer.status != MTLCommandBufferStatusCompleted)
    {
        FString ErrorDescription = TEXT("none");
        if(CommandBuffer.error != nil && CommandBuffer.error.localizedDescription != nil)
        {
            ErrorDescription = FString(UTF8_TO_TCHAR(CommandBuffer.error.localizedDescription.UTF8String));
        }
        OutDiagnostics.Message = FString::Printf(TEXT("Metal IOSurface intermediate blit failed: command buffer status=%ld error='%s'"), static_cast<long>(CommandBuffer.status), *ErrorDescription);
        return false;
    }

    return true;
}

} // namespace

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


FNozzleMetalTextureDiagnostics NozzleUnrealDescribeMetalTexture(void* NativeTexture, const TCHAR* TextureLabel)
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
            TEXT("%s MTLPixelFormat=%llu width=%d height=%d storageMode=%llu usage=0x%llx iosurface_backed=%s iosurface_id=%lld"),
            TextureLabel != nullptr ? TextureLabel : TEXT("texture"),
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

void NozzleUnrealReleaseMetalIntermediateTextureCache(FNozzleMetalIntermediateTextureCache& Cache)
{
    @autoreleasepool
    {
        NozzleUnrealClearMetalIntermediateCache(Cache);
    }
}

bool NozzleUnrealPrepareMetalPublishTexture(
    const FNozzleNativeTextureView& SourceTextureView,
    const FNozzleNativeDeviceView& DeviceView,
    FNozzleMetalIntermediateTextureCache& Cache,
    FNozzleNativeTextureView& OutPublishTextureView,
    FNozzleRuntimeDiagnostics& OutDiagnostics)
{
    OutPublishTextureView = FNozzleNativeTextureView{};
    if(SourceTextureView.NativeTexture == nullptr)
    {
        OutDiagnostics.Message = TEXT("Metal publish preparation failed: source texture is null");
        return false;
    }
    if(DeviceView.Device == nullptr)
    {
        OutDiagnostics.Message = TEXT("Metal publish preparation failed: Metal device is null");
        return false;
    }

    @autoreleasepool
    {
        id<MTLTexture> SourceTexture = (__bridge id<MTLTexture>)SourceTextureView.NativeTexture;
        id<MTLDevice> Device = (__bridge id<MTLDevice>)DeviceView.Device;
        if(SourceTexture == nil || Device == nil)
        {
            OutDiagnostics.Message = TEXT("Metal publish preparation failed: bridged source texture or device is nil");
            return false;
        }
        if(SourceTexture.device != Device)
        {
            OutDiagnostics.Message = TEXT("Metal publish preparation failed: source texture device does not match captured Metal device");
            return false;
        }
        const bool bSourceFormatIsBGRA8 = SourceTexture.pixelFormat == MTLPixelFormatBGRA8Unorm || SourceTexture.pixelFormat == MTLPixelFormatBGRA8Unorm_sRGB;
        if(!bSourceFormatIsBGRA8)
        {
            OutDiagnostics.Message = FString::Printf(TEXT("Metal publish preparation failed: unsupported source MTLPixelFormat=%llu; only BGRA8Unorm/BGRA8Unorm_sRGB byte-compatible sources are supported by the IOSurface intermediate path"), static_cast<unsigned long long>(SourceTexture.pixelFormat));
            return false;
        }

        const FNozzleMetalTextureDiagnostics SourceDiagnostics = NozzleUnrealDescribeMetalTexture(SourceTextureView.NativeTexture, TEXT("source=FRHITexture::GetNativeResource"));
        if(SourceDiagnostics.bIOSurfaceBacked && 0 < SourceDiagnostics.IOSurfaceID)
        {
            OutPublishTextureView = SourceTextureView;
            OutPublishTextureView.TransferMode = TEXT("unreal_metal_direct_iosurface_source_to_nozzle_ring");
            OutPublishTextureView.SynchronizationBoundary = TEXT("render thread calls RHICmdList.SubmitAndBlockUntilGPUIdle before publish; source texture is IOSurface-backed; nozzle_sender_publish_native_texture_ex performs a synchronous Metal blit into the nozzle-owned ring texture and waits for command-buffer completion");
            OutDiagnostics.NativeTextureDetails = SourceDiagnostics.Details;
            OutDiagnostics.bIOSurfaceBacked = true;
            OutDiagnostics.IOSurfaceID = SourceDiagnostics.IOSurfaceID;
            OutDiagnostics.TransferMode = OutPublishTextureView.TransferMode;
            OutDiagnostics.SynchronizationBoundary = OutPublishTextureView.SynchronizationBoundary;
            OutDiagnostics.Message = TEXT("Metal publish preparation using direct IOSurface-backed Unreal source texture");
            return true;
        }

        if(!NozzleUnrealEnsureMetalIntermediateTexture(Device, SourceTextureView.Width, SourceTextureView.Height, Cache, OutDiagnostics))
        {
            return false;
        }
        if(!NozzleUnrealBlitMetalTextureToIntermediate(SourceTexture, Cache, OutDiagnostics))
        {
            return false;
        }

        OutPublishTextureView.NativeTexture = Cache.Texture;
        OutPublishTextureView.Width = Cache.Width;
        OutPublishTextureView.Height = Cache.Height;
        OutPublishTextureView.TransferMode = TEXT("unreal_metal_blit_to_iosurface");
        OutPublishTextureView.SynchronizationBoundary = TEXT("render thread calls RHICmdList.SubmitAndBlockUntilGPUIdle before the cross-queue copy; Unreal source MTLTexture was copied into a named kIOSurfaceIsGlobal BGRA8Unorm intermediate with a Metal blit command buffer and waitUntilCompleted; nozzle_sender_publish_native_texture_ex then performs its synchronous Metal blit into the nozzle-owned ring texture");

        const FNozzleMetalTextureDiagnostics IntermediateDiagnostics = NozzleUnrealDescribeMetalTexture(OutPublishTextureView.NativeTexture, TEXT("published_intermediate"));
        OutDiagnostics.NativeTextureDetails = FString::Printf(TEXT("source={%s}; published_intermediate={%s}"), *SourceDiagnostics.Details, *IntermediateDiagnostics.Details);
        OutDiagnostics.bIOSurfaceBacked = IntermediateDiagnostics.bIOSurfaceBacked;
        OutDiagnostics.IOSurfaceID = IntermediateDiagnostics.IOSurfaceID;
        OutDiagnostics.Width = Cache.Width;
        OutDiagnostics.Height = Cache.Height;
        OutDiagnostics.TransferMode = OutPublishTextureView.TransferMode;
        OutDiagnostics.SynchronizationBoundary = OutPublishTextureView.SynchronizationBoundary;
        OutDiagnostics.Message = TEXT("Metal publish preparation copied Unreal source texture into IOSurface-backed intermediate");
        return IntermediateDiagnostics.bIOSurfaceBacked && 0 < IntermediateDiagnostics.IOSurfaceID;
    }
}
#endif
