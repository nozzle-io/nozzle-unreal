#include "NozzleSmoke.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "NozzleDiagnostics.h"
#include "NozzleReceiverComponent.h"
#include "NozzleSenderComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

#include <cstdlib>

#include "Containers/Set.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogNozzleSmoke, Log, All);

namespace
{

constexpr int32 NozzleSmokeFrameCount = 180;
constexpr int32 NozzleSmokeMarkerWidth = 24;
constexpr int32 NozzleSmokeMarkerHeight = 32;
constexpr int32 NozzleSmokeMarkerY = 128;

struct FNozzleSmokeScenario
{
    int32 Width = 320;
    int32 Height = 240;
    int32 FrameCount = NozzleSmokeFrameCount;
    FString SenderName = TEXT("NozzleUnrealSmoke320");
};

FString NozzleSmokeDiagnosticsToString(const FNozzleRuntimeDiagnostics& Diagnostics)
{
    return FString::Printf(
        TEXT("state=%d can_use_runtime=%d backend='%s' transfer='%s' width=%d height=%d message='%s' native='%s' sync='%s' iosurface_backed=%d iosurface_id=%llu"),
        static_cast<int32>(Diagnostics.State),
        Diagnostics.bCanUseRuntime ? 1 : 0,
        *Diagnostics.Backend,
        *Diagnostics.TransferMode,
        Diagnostics.Width,
        Diagnostics.Height,
        *Diagnostics.Message,
        *Diagnostics.NativeTextureDetails,
        *Diagnostics.SynchronizationBoundary,
        Diagnostics.bIOSurfaceBacked ? 1 : 0,
        static_cast<unsigned long long>(Diagnostics.IOSurfaceID));
}

UWorld* FindNozzleSmokeWorld(EWorldType::Type RequiredWorldType)
{
    if(GEngine == nullptr)
    {
        return nullptr;
    }

    UWorld* FoundWorld = nullptr;
    for(const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
        if(WorldContext.WorldType == RequiredWorldType && WorldContext.World() != nullptr)
        {
            if(FoundWorld != nullptr)
            {
                return nullptr;
            }
            FoundWorld = WorldContext.World();
        }
    }

    return FoundWorld;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
UWorld* FindNozzleSmokePIEWorld()
{
    if(GEditor != nullptr && GEditor->PlayWorld != nullptr)
    {
        return GEditor->PlayWorld;
    }

    return FindNozzleSmokeWorld(EWorldType::PIE);
}
#endif

UWorld* FindNozzleSmokeGameWorld()
{
    return FindNozzleSmokeWorld(EWorldType::Game);
}

void FillNozzleSmokeRect(TArray<uint8>& Pixels, int32 TextureWidth, int32 TextureHeight, int32 X, int32 Y, int32 Width, int32 Height, uint8 Red, uint8 Green, uint8 Blue, uint8 Alpha)
{
    const int32 StartX = FMath::Clamp(X, 0, TextureWidth);
    const int32 StartY = FMath::Clamp(Y, 0, TextureHeight);
    const int32 EndX = FMath::Clamp(X + Width, 0, TextureWidth);
    const int32 EndY = FMath::Clamp(Y + Height, 0, TextureHeight);

    for(int32 Row = StartY; Row < EndY; Row++)
    {
        for(int32 Column = StartX; Column < EndX; Column++)
        {
            const int32 Offset = ((Row * TextureWidth) + Column) * 4;
            Pixels[Offset + 0] = Blue;
            Pixels[Offset + 1] = Green;
            Pixels[Offset + 2] = Red;
            Pixels[Offset + 3] = Alpha;
        }
    }
}

bool DrawNozzleSmokePattern(UTextureRenderTarget2D* RenderTarget, const FNozzleSmokeScenario& Scenario, int32 FrameIndex)
{
    if(RenderTarget == nullptr)
    {
        return false;
    }

    FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
    if(RenderTargetResource == nullptr)
    {
        return false;
    }

    TSharedRef<TArray<uint8>, ESPMode::ThreadSafe> Pixels = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
    Pixels->SetNumZeroed(Scenario.Width * Scenario.Height * 4);
    for(int32 Index = 0; Index < Scenario.Width * Scenario.Height; Index++)
    {
        const int32 Offset = Index * 4;
        (*Pixels)[Offset + 0] = 5;
        (*Pixels)[Offset + 1] = 5;
        (*Pixels)[Offset + 2] = 5;
        (*Pixels)[Offset + 3] = 255;
    }

    const int32 CornerWidth = FMath::Max(32, Scenario.Width / 3);
    const int32 CornerHeight = FMath::Max(32, Scenario.Height / 3);
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, 0, 0, CornerWidth, CornerHeight, 255, 0, 0, 255);
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, Scenario.Width - CornerWidth, 0, CornerWidth, CornerHeight, 0, 255, 0, 255);
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, 0, Scenario.Height - CornerHeight, CornerWidth, CornerHeight, 0, 0, 255, 255);
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, Scenario.Width - CornerWidth, Scenario.Height - CornerHeight, CornerWidth, CornerHeight, 255, 255, 255, 255);

    const int32 AlphaPatchWidth = FMath::Max(24, Scenario.Width / 7);
    const int32 AlphaPatchHeight = FMath::Max(16, Scenario.Height / 8);
    const int32 AlphaPatchX = (Scenario.Width / 2) - (AlphaPatchWidth / 2);
    const int32 AlphaPatchY = ((Scenario.Height / 2) - (Scenario.Height / 16)) - (AlphaPatchHeight / 2);
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, AlphaPatchX, AlphaPatchY, AlphaPatchWidth, AlphaPatchHeight, 255, 0, 255, 64);

    const int32 MarkerTravelWidth = FMath::Max(1, Scenario.Width - NozzleSmokeMarkerWidth);
    const int32 MarkerX = (FrameIndex * 29) % MarkerTravelWidth;
    FillNozzleSmokeRect(*Pixels, Scenario.Width, Scenario.Height, MarkerX, NozzleSmokeMarkerY, NozzleSmokeMarkerWidth, NozzleSmokeMarkerHeight, 255, 255, 0, 255);

    FTextureRHIRef TextureRHI = RenderTargetResource->GetRenderTargetTexture();
    if(!TextureRHI.IsValid())
    {
        return false;
    }

    ENQUEUE_RENDER_COMMAND(NozzleSmokeUpdateRenderTarget)(
        [TextureRHI, Pixels, Scenario](FRHICommandListImmediate& RHICmdList)
        {
            const FUpdateTextureRegion2D Region(0, 0, 0, 0, Scenario.Width, Scenario.Height);
            RHICmdList.UpdateTexture2D(TextureRHI, 0, Region, Scenario.Width * 4, Pixels->GetData());
        });
    return true;
}

struct FNozzleSmokeReceiverSample
{
    FString Name;
    int32 X = 0;
    int32 Y = 0;
    FColor Expected;
    FColor Actual;
    bool bPassed = false;
};

bool NozzleSmokeSampleColor(const TArray<FColor>& Pixels, int32 Width, int32 Height, const TCHAR* Name, int32 X, int32 Y, FColor Expected, FNozzleSmokeReceiverSample& OutSample, bool bCheckAlpha = true)
{
    OutSample.Name = Name;
    OutSample.X = X;
    OutSample.Y = Y;
    OutSample.Expected = Expected;
    const int64 ExpectedPixelCount = static_cast<int64>(Width) * static_cast<int64>(Height);
    if(X < 0 || Y < 0 || Width <= X || Height <= Y || static_cast<int64>(Pixels.Num()) != ExpectedPixelCount)
    {
        OutSample.Actual = FColor::Transparent;
        OutSample.bPassed = false;
        return false;
    }
    OutSample.Actual = Pixels[(static_cast<int64>(Y) * static_cast<int64>(Width)) + static_cast<int64>(X)];
    OutSample.bPassed = OutSample.Actual.R == Expected.R
        && OutSample.Actual.G == Expected.G
        && OutSample.Actual.B == Expected.B
        && (!bCheckAlpha || OutSample.Actual.A == Expected.A);
    return OutSample.bPassed;
}

FString NozzleSmokeSamplesToString(const TArray<FNozzleSmokeReceiverSample>& Samples)
{
    FString Result;
    for(const FNozzleSmokeReceiverSample& Sample : Samples)
    {
        Result += FString::Printf(
            TEXT("%s@(%d,%d) expected=[%d,%d,%d,%d] actual=[%d,%d,%d,%d] passed=%d; "),
            *Sample.Name,
            Sample.X,
            Sample.Y,
            Sample.Expected.R,
            Sample.Expected.G,
            Sample.Expected.B,
            Sample.Expected.A,
            Sample.Actual.R,
            Sample.Actual.G,
            Sample.Actual.B,
            Sample.Actual.A,
            Sample.bPassed ? 1 : 0);
    }
    return Result;
}

bool NozzleSmokeFindYellowMarkerX(const TArray<FColor>& Pixels, int32 Width, int32 Height, int32& OutMarkerX, bool bCheckAlpha = true)
{
    constexpr int32 MarkerY = 144;
    const int64 ExpectedPixelCount = static_cast<int64>(Width) * static_cast<int64>(Height);
    if(Width <= 24 || Height <= MarkerY || static_cast<int64>(Pixels.Num()) != ExpectedPixelCount)
    {
        return false;
    }
    for(int32 X = 0; X < Width; X++)
    {
        const FColor Pixel = Pixels[(static_cast<int64>(MarkerY) * static_cast<int64>(Width)) + static_cast<int64>(X)];
        if(Pixel.R == 255 && Pixel.G == 255 && Pixel.B == 0 && (!bCheckAlpha || Pixel.A == 255))
        {
            OutMarkerX = X;
            return true;
        }
    }
    return false;
}


class FNozzleSmokeRuntimeRunner final
{
public:
    explicit FNozzleSmokeRuntimeRunner(const FNozzleSmokeScenario& InScenario, bool bInRequireStrictPass)
    : Scenario(InScenario)
    , bRequireStrictPass(bInRequireStrictPass)
    {}

    bool Tick()
    {
        if(bFinished)
        {
            return false;
        }

        UWorld* GameWorld = FindNozzleSmokeGameWorld();
        if(GameWorld == nullptr)
        {
            const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
            if(ElapsedSeconds < GameWorldTimeoutSeconds)
            {
                return true;
            }
            Finish(false, *FString::Printf(TEXT("game world was not available after %.2f seconds"), ElapsedSeconds));
            return false;
        }

        if(RenderTarget == nullptr)
        {
            SenderActor = GameWorld->SpawnActor<AActor>();
            if(SenderActor == nullptr)
            {
                Finish(false, TEXT("SpawnActor returned null"));
                return false;
            }

            SenderComponent = NewObject<UNozzleSenderComponent>(SenderActor, TEXT("NozzleSmokePackagedSenderComponent"));
            if(SenderComponent == nullptr)
            {
                Finish(false, TEXT("failed to create sender component"));
                return false;
            }

            RenderTarget = NewObject<UTextureRenderTarget2D>(SenderComponent, NAME_None);
            if(RenderTarget == nullptr)
            {
                Finish(false, TEXT("failed to create render target"));
                return false;
            }

            RenderTarget->RenderTargetFormat = RTF_RGBA8;
            RenderTarget->ClearColor = FLinearColor::Black;
            RenderTarget->bAutoGenerateMips = false;
            RenderTarget->bForceLinearGamma = true;
            RenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
            RenderTarget->UpdateResourceImmediate(true);

            SenderActor->AddInstanceComponent(SenderComponent);
            SenderComponent->SenderName = Scenario.SenderName;
            SenderComponent->SourceRenderTarget = RenderTarget;
            SenderComponent->RegisterComponent();

            const bool bStarted = SenderComponent->StartSender();
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_START packaged=1 started=%d diagnostics=%s"), bStarted ? 1 : 0, *NozzleSmokeDiagnosticsToString(SenderComponent->GetLastDiagnostics()));
            if(!bStarted)
            {
                Finish(false, TEXT("StartSender returned false"));
                return false;
            }
        }

        if(!DrawNozzleSmokePattern(RenderTarget, Scenario, PublishedFrames))
        {
            PatternUploadAttempts += 1;
            if(PatternUploadAttempts < 30)
            {
                return true;
            }
            Finish(false, TEXT("unable to enqueue render target pattern upload"));
            return false;
        }
        PatternUploadAttempts = 0;

        FlushRenderingCommands();
        const bool bQueued = SenderComponent->PublishFrame();
        FlushRenderingCommands();
        const FNozzleRuntimeDiagnostics RenderDiagnostics = SenderComponent->GetLastRenderDiagnostics();
        const int64 RenderSequence = SenderComponent->GetLastRenderSequence();

        if(PublishedFrames < 3 || (PublishedFrames % 30) == 0)
        {
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_FRAME packaged=1 frame=%d queued=%d render_sequence=%lld diagnostics=%s"), PublishedFrames, bQueued ? 1 : 0, static_cast<long long>(RenderSequence), *NozzleSmokeDiagnosticsToString(RenderDiagnostics));
        }

        if(!bQueued)
        {
            Finish(false, *FString::Printf(TEXT("PublishFrame returned false at frame=%d"), PublishedFrames));
            return false;
        }

        LastRenderSequence = RenderSequence;
        LastDiagnostics = RenderDiagnostics;
        PublishedFrames += 1;

        if(PublishedFrames < Scenario.FrameCount)
        {
            return true;
        }

        const bool bPublishedMultipleFrames = Scenario.FrameCount <= LastRenderSequence;
        const bool bHasExpectedSize = LastDiagnostics.Width == Scenario.Width && LastDiagnostics.Height == Scenario.Height;
        const bool bIOSurfaceBacked = LastDiagnostics.bIOSurfaceBacked && 0 < LastDiagnostics.IOSurfaceID;
        const bool bRenderDiagnosticsRunning = LastDiagnostics.State == ENozzleRuntimeState::Running && LastDiagnostics.bCanUseRuntime;
        const bool bPassCandidate = bPublishedMultipleFrames && bHasExpectedSize && bIOSurfaceBacked && bRenderDiagnosticsRunning;

        const TCHAR* RowStatus = bPassCandidate ? TEXT("PASS_CANDIDATE") : TEXT("MISSING");
        UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_RESULT packaged=1 row_status=%s frames=%d last_sequence=%lld expected_size=%d iosurface_backed=%d iosurface_id=%llu render_running=%d strict=%d final=%s"), RowStatus, PublishedFrames, static_cast<long long>(LastRenderSequence), bHasExpectedSize ? 1 : 0, bIOSurfaceBacked ? 1 : 0, static_cast<unsigned long long>(LastDiagnostics.IOSurfaceID), bRenderDiagnosticsRunning ? 1 : 0, bRequireStrictPass ? 1 : 0, *NozzleSmokeDiagnosticsToString(LastDiagnostics));

        const bool bSuccess = bPassCandidate || (!bRequireStrictPass && bPublishedMultipleFrames && bHasExpectedSize);
        Finish(bSuccess, bSuccess ? TEXT("completed") : TEXT("strict packaged sender checks failed"));
        return false;
    }

private:
    void Finish(bool bSuccess, const TCHAR* Message)
    {
        bFinished = true;
        UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_EXIT packaged=1 success=%d message='%s'"), bSuccess ? 1 : 0, Message);
        FPlatformMisc::RequestExitWithStatus(false, bSuccess ? EXIT_SUCCESS : EXIT_FAILURE);
        if(!bSuccess)
        {
            std::exit(EXIT_FAILURE);
        }
    }

    FNozzleSmokeScenario Scenario;
    bool bRequireStrictPass = false;
    bool bFinished = false;
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
    TObjectPtr<AActor> SenderActor = nullptr;
    TObjectPtr<UNozzleSenderComponent> SenderComponent = nullptr;
    FNozzleRuntimeDiagnostics LastDiagnostics;
    int64 LastRenderSequence = 0;
    double StartSeconds = FPlatformTime::Seconds();
    static constexpr double GameWorldTimeoutSeconds = 60.0;
    int32 PatternUploadAttempts = 0;
    int32 PublishedFrames = 0;
};


class FNozzleSmokePackagedReceiveRunner final
{
public:
    explicit FNozzleSmokePackagedReceiveRunner(const FNozzleSmokeScenario& InScenario, bool bInRequireStrictPass)
    : Scenario(InScenario)
    , bRequireStrictPass(bInRequireStrictPass)
    {}

    bool Tick()
    {
        if(bFinished)
        {
            return false;
        }

        UWorld* GameWorld = FindNozzleSmokeGameWorld();
        if(GameWorld == nullptr)
        {
            const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
            if(ElapsedSeconds < GameWorldTimeoutSeconds)
            {
                return true;
            }
            Finish(false, *FString::Printf(TEXT("game world was not available after %.2f seconds"), ElapsedSeconds));
            return false;
        }

        if(RenderTarget == nullptr)
        {
            if(!Initialize(GameWorld))
            {
                return false;
            }
        }

        const bool bPolled = ReceiverComponent->PollFrame();
        FlushRenderingCommands();
        const FNozzleRuntimeDiagnostics RenderDiagnostics = ReceiverComponent->GetLastRenderDiagnostics();
        const int64 RenderSequence = ReceiverComponent->GetLastRenderSequence();
        if(RenderSequence != LastRenderSequence)
        {
            DistinctRenderSequences += 1;
            LastRenderSequence = RenderSequence;
            LastDiagnostics = RenderDiagnostics;
        }

        bool bMaterialDraw = false;
        if(ReceiverMaterialInstance != nullptr && MaterialRenderTarget != nullptr)
        {
            UKismetRenderingLibrary::DrawMaterialToRenderTarget(GameWorld, MaterialRenderTarget, ReceiverMaterialInstance);
            FlushRenderingCommands();
            bMaterialDraw = true;
        }

        TArray<FColor> MaterialPixels;
        FTextureRenderTargetResource* MaterialRenderTargetResource = MaterialRenderTarget != nullptr ? MaterialRenderTarget->GameThread_GetRenderTargetResource() : nullptr;
        const bool bReadMaterialPixels = MaterialRenderTargetResource != nullptr && MaterialRenderTargetResource->ReadPixels(MaterialPixels);
        TArray<FColor> DirectPixels;
        FTextureRenderTargetResource* DirectRenderTargetResource = RenderTarget != nullptr ? RenderTarget->GameThread_GetRenderTargetResource() : nullptr;
        const bool bReadDirectPixels = DirectRenderTargetResource != nullptr && DirectRenderTargetResource->ReadPixels(DirectPixels);

        TArray<FNozzleSmokeReceiverSample> MaterialSamples;
        TArray<FNozzleSmokeReceiverSample> DirectSamples;
        bool bMaterialPatternOk = false;
        bool bDirectPatternOk = false;
        bool bMarkerFound = false;
        int32 MarkerX = -1;
        EvaluateSamples(MaterialPixels, bReadMaterialPixels, false, MaterialSamples, bMaterialPatternOk, bMarkerFound, MarkerX);
        if(bMarkerFound)
        {
            MarkerXs.Add(MarkerX);
        }
        bool bDirectMarkerFound = false;
        int32 DirectMarkerX = -1;
        EvaluateSamples(DirectPixels, bReadDirectPixels, true, DirectSamples, bDirectPatternOk, bDirectMarkerFound, DirectMarkerX);

        if(ObservedFrames < 3 || (ObservedFrames % 30) == 0)
        {
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_RECEIVER_SMOKE_FRAME packaged=1 frame=%d polled=%d material_draw=%d read_pixels=%d read_direct_pixels=%d render_sequence=%lld material_pattern_ok=%d direct_pattern_ok=%d marker_found=%d marker_x=%d diagnostics=%s material_samples=%s direct_samples=%s"),
                ObservedFrames,
                bPolled ? 1 : 0,
                bMaterialDraw ? 1 : 0,
                bReadMaterialPixels ? 1 : 0,
                bReadDirectPixels ? 1 : 0,
                static_cast<long long>(RenderSequence),
                bMaterialPatternOk ? 1 : 0,
                bDirectPatternOk ? 1 : 0,
                bMarkerFound ? 1 : 0,
                MarkerX,
                *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
                *NozzleSmokeSamplesToString(MaterialSamples),
                *NozzleSmokeSamplesToString(DirectSamples));
        }
        ObservedFrames += 1;

        const bool bExpectedSize = RenderDiagnostics.Width == Scenario.Width && RenderDiagnostics.Height == Scenario.Height;
        const bool bRenderDiagnosticsRunning = RenderDiagnostics.State == ENozzleRuntimeState::Running && RenderDiagnostics.bCanUseRuntime;
        const bool bMetalRHI = RenderDiagnostics.bMetalRHI && RenderDiagnostics.Backend == TEXT("Metal");
        const bool bNativeDetailsOk = RenderDiagnostics.NativeTextureDetails.Contains(TEXT("receiver_target=FRHITexture::GetNativeResource"))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("MTLPixelFormat=81"))
            && RenderDiagnostics.NativeTextureDetails.Contains(FString::Printf(TEXT("width=%d"), Scenario.Width))
            && RenderDiagnostics.NativeTextureDetails.Contains(FString::Printf(TEXT("height=%d"), Scenario.Height))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("storageMode="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("usage="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("iosurface_backed="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("iosurface_id="));
        const bool bTransferPathNamed = RenderDiagnostics.TransferMode == TEXT("nozzle_frame_to_unreal_metal_texture") && bNativeDetailsOk;
        const bool bSyncBoundaryNamed = RenderDiagnostics.SynchronizationBoundary.Contains(TEXT("nozzle_frame_copy_to_native_texture"))
            && RenderDiagnostics.SynchronizationBoundary.Contains(TEXT("backend Metal copy wait completes"));
        const bool bMarkerMoved = 1 < MarkerXs.Num();
        const bool bEnoughFrames = RequiredDistinctSequences <= DistinctRenderSequences;
        const bool bPassCandidate = bExpectedSize && bRenderDiagnosticsRunning && bMetalRHI && bTransferPathNamed && bSyncBoundaryNamed && bMaterialDraw && bReadMaterialPixels && bReadDirectPixels && bMaterialPatternOk && bDirectPatternOk && bMarkerFound && bMarkerMoved && bEnoughFrames;
        if(bPassCandidate)
        {
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_RECEIVER_SMOKE_RESULT packaged=1 row_status=PASS_CANDIDATE source='%s' frames=%d distinct_sequences=%d marker_positions=%d last_marker_x=%d expected_size=%d metal=%d material_draw=%d material_pattern_ok=%d direct_rgba_ok=%d transfer_path=%d native_details=%d sync=%d strict=%d final=%s material_samples=%s direct_samples=%s"),
                *Scenario.SenderName,
                ObservedFrames,
                DistinctRenderSequences,
                MarkerXs.Num(),
                MarkerX,
                bExpectedSize ? 1 : 0,
                bMetalRHI ? 1 : 0,
                bMaterialDraw ? 1 : 0,
                bMaterialPatternOk ? 1 : 0,
                bDirectPatternOk ? 1 : 0,
                bTransferPathNamed ? 1 : 0,
                bNativeDetailsOk ? 1 : 0,
                bSyncBoundaryNamed ? 1 : 0,
                bRequireStrictPass ? 1 : 0,
                *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
                *NozzleSmokeSamplesToString(MaterialSamples),
                *NozzleSmokeSamplesToString(DirectSamples));
            Finish(true, TEXT("completed"));
            return false;
        }

        const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
        if(ElapsedSeconds < ReceiverTimeoutSeconds)
        {
            return true;
        }

        UE_LOG(LogNozzleSmoke, Error, TEXT("NOZZLE_RECEIVER_SMOKE_RESULT packaged=1 row_status=MISSING source='%s' frames=%d distinct_sequences=%d marker_positions=%d last_marker_x=%d expected_size=%d material_draw=%d read_pixels=%d read_direct_pixels=%d material_pattern_ok=%d direct_rgba_ok=%d marker_found=%d metal=%d transfer_path=%d native_details=%d sync=%d final=%s material_samples=%s direct_samples=%s"),
            *Scenario.SenderName,
            ObservedFrames,
            DistinctRenderSequences,
            MarkerXs.Num(),
            MarkerX,
            bExpectedSize ? 1 : 0,
            bMaterialDraw ? 1 : 0,
            bReadMaterialPixels ? 1 : 0,
            bReadDirectPixels ? 1 : 0,
            bMaterialPatternOk ? 1 : 0,
            bDirectPatternOk ? 1 : 0,
            bMarkerFound ? 1 : 0,
            bMetalRHI ? 1 : 0,
            bTransferPathNamed ? 1 : 0,
            bNativeDetailsOk ? 1 : 0,
            bSyncBoundaryNamed ? 1 : 0,
            *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
            *NozzleSmokeSamplesToString(MaterialSamples),
            *NozzleSmokeSamplesToString(DirectSamples));
        Finish(false, TEXT("packaged receiver/material checks timed out"));
        return false;
    }

private:
    bool Initialize(UWorld* GameWorld)
    {
        ReceiverActor = GameWorld->SpawnActor<AActor>();
        if(ReceiverActor == nullptr)
        {
            Finish(false, TEXT("SpawnActor returned null"));
            return false;
        }

        ReceiverComponent = NewObject<UNozzleReceiverComponent>(ReceiverActor, TEXT("NozzleSmokePackagedReceiverComponent"));
        if(ReceiverComponent == nullptr)
        {
            Finish(false, TEXT("failed to create receiver component"));
            return false;
        }

        RenderTarget = NewObject<UTextureRenderTarget2D>(ReceiverComponent, TEXT("NozzleSmokePackagedReceiverRenderTarget"));
        if(RenderTarget == nullptr)
        {
            Finish(false, TEXT("failed to create receiver render target"));
            return false;
        }
        RenderTarget->RenderTargetFormat = RTF_RGBA8;
        RenderTarget->ClearColor = FLinearColor::Black;
        RenderTarget->bAutoGenerateMips = false;
        RenderTarget->bForceLinearGamma = true;
        RenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
        RenderTarget->UpdateResourceImmediate(true);

        MaterialRenderTarget = NewObject<UTextureRenderTarget2D>(ReceiverComponent, TEXT("NozzleSmokePackagedMaterialRenderTarget"));
        if(MaterialRenderTarget == nullptr)
        {
            Finish(false, TEXT("failed to create material render target"));
            return false;
        }
        MaterialRenderTarget->RenderTargetFormat = RTF_RGBA8;
        MaterialRenderTarget->ClearColor = FLinearColor::Black;
        MaterialRenderTarget->bAutoGenerateMips = false;
        MaterialRenderTarget->bForceLinearGamma = true;
        MaterialRenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
        MaterialRenderTarget->UpdateResourceImmediate(true);

        UMaterial* ReceiverMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/NozzleSmoke/NozzleSmokeReceiverMaterial.NozzleSmokeReceiverMaterial"));
        if(ReceiverMaterial == nullptr)
        {
            Finish(false, TEXT("failed to load /Game/NozzleSmoke/NozzleSmokeReceiverMaterial material asset"));
            return false;
        }
        ReceiverMaterialInstance = UMaterialInstanceDynamic::Create(ReceiverMaterial, ReceiverComponent);
        if(ReceiverMaterialInstance == nullptr)
        {
            Finish(false, TEXT("failed to create receiver material instance"));
            return false;
        }
        ReceiverMaterialInstance->SetTextureParameterValue(TEXT("ReceiverTexture"), RenderTarget);

        ReceiverActor->AddInstanceComponent(ReceiverComponent);
        ReceiverComponent->SenderName = Scenario.SenderName;
        ReceiverComponent->TargetRenderTarget = RenderTarget;
        ReceiverComponent->AcquireTimeoutMs = 100;
        ReceiverComponent->RegisterComponent();
        UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_RECEIVER_SMOKE_START packaged=1 source='%s' width=%d height=%d material='/Game/NozzleSmoke/NozzleSmokeReceiverMaterial' diagnostics=%s"), *Scenario.SenderName, Scenario.Width, Scenario.Height, *NozzleSmokeDiagnosticsToString(ReceiverComponent->GetLastDiagnostics()));
        return true;
    }

    void EvaluateSamples(const TArray<FColor>& Pixels, bool bReadPixels, bool bCheckAlpha, TArray<FNozzleSmokeReceiverSample>& OutSamples, bool& bOutPatternOk, bool& bOutMarkerFound, int32& OutMarkerX) const
    {
        bOutPatternOk = false;
        bOutMarkerFound = false;
        OutMarkerX = -1;
        if(!bReadPixels)
        {
            return;
        }

        const int32 LeftX = Scenario.Width / 8;
        const int32 RightX = Scenario.Width - 1 - (Scenario.Width / 8);
        const int32 TopY = Scenario.Height / 8;
        const int32 BottomY = Scenario.Height - 1 - (Scenario.Height / 8);
        const int32 AlphaX = Scenario.Width / 2;
        const int32 AlphaY = (Scenario.Height / 2) - (Scenario.Height / 16);
        OutSamples.SetNum(5);
        const bool bTopLeftRed = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, bCheckAlpha ? TEXT("direct_top_left_red_rgba") : TEXT("material_top_left_red_rgb"), LeftX, TopY, FColor(255, 0, 0, 255), OutSamples[0], bCheckAlpha);
        const bool bTopRightGreen = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, bCheckAlpha ? TEXT("direct_top_right_green_rgba") : TEXT("material_top_right_green_rgb"), RightX, TopY, FColor(0, 255, 0, 255), OutSamples[1], bCheckAlpha);
        const bool bBottomLeftBlue = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, bCheckAlpha ? TEXT("direct_bottom_left_blue_rgba") : TEXT("material_bottom_left_blue_rgb"), LeftX, BottomY, FColor(0, 0, 255, 255), OutSamples[2], bCheckAlpha);
        const bool bBottomRightWhite = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, bCheckAlpha ? TEXT("direct_bottom_right_white_rgba") : TEXT("material_bottom_right_white_rgb"), RightX, BottomY, FColor(255, 255, 255, 255), OutSamples[3], bCheckAlpha);
        const bool bAlphaPatch = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, bCheckAlpha ? TEXT("direct_center_magenta_alpha_patch_rgba") : TEXT("material_center_magenta_alpha_patch_rgb"), AlphaX, AlphaY, FColor(255, 0, 255, 64), OutSamples[4], bCheckAlpha);
        bOutPatternOk = bTopLeftRed && bTopRightGreen && bBottomLeftBlue && bBottomRightWhite && bAlphaPatch;
        bOutMarkerFound = NozzleSmokeFindYellowMarkerX(Pixels, Scenario.Width, Scenario.Height, OutMarkerX, bCheckAlpha);
    }

    void Finish(bool bSuccess, const TCHAR* Message)
    {
        bFinished = true;
        UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_RECEIVER_SMOKE_EXIT packaged=1 success=%d message='%s'"), bSuccess ? 1 : 0, Message);
        FPlatformMisc::RequestExitWithStatus(false, bSuccess ? EXIT_SUCCESS : EXIT_FAILURE);
        if(!bSuccess)
        {
            std::exit(EXIT_FAILURE);
        }
    }

    FNozzleSmokeScenario Scenario;
    bool bRequireStrictPass = false;
    bool bFinished = false;
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
    TObjectPtr<UTextureRenderTarget2D> MaterialRenderTarget = nullptr;
    TObjectPtr<UMaterialInstanceDynamic> ReceiverMaterialInstance = nullptr;
    TObjectPtr<AActor> ReceiverActor = nullptr;
    TObjectPtr<UNozzleReceiverComponent> ReceiverComponent = nullptr;
    FNozzleRuntimeDiagnostics LastDiagnostics;
    int64 LastRenderSequence = 0;
    int32 ObservedFrames = 0;
    int32 DistinctRenderSequences = 0;
    TSet<int32> MarkerXs;
    double StartSeconds = FPlatformTime::Seconds();
    static constexpr int32 RequiredDistinctSequences = 6;
    static constexpr double GameWorldTimeoutSeconds = 60.0;
    static constexpr double ReceiverTimeoutSeconds = 90.0;
};

} // namespace

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
class FNozzleSmokePublishLatentCommand final : public IAutomationLatentCommand
{
public:
    FNozzleSmokePublishLatentCommand(FAutomationTestBase* InTest, const FNozzleSmokeScenario& InScenario, bool bInRequireStrictPass)
    : Test(InTest)
    , Scenario(InScenario)
    , bRequireStrictPass(bInRequireStrictPass)
    {}

    virtual bool Update() override
    {
        if(Test == nullptr)
        {
            return true;
        }

        UWorld* PIEWorld = FindNozzleSmokePIEWorld();
        if(PIEWorld == nullptr)
        {
            Attempts += 1;
            if(Attempts < 120)
            {
                return false;
            }
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: PIE world was not available"));
            return true;
        }

        if(RenderTarget == nullptr)
        {
            SenderActor = PIEWorld->SpawnActor<AActor>();
            if(SenderActor == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: SpawnActor returned null"));
                return true;
            }

            SenderComponent = NewObject<UNozzleSenderComponent>(SenderActor, TEXT("NozzleSmokeSenderComponent"));
            if(SenderComponent == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: failed to create sender component"));
                return true;
            }

            RenderTarget = NewObject<UTextureRenderTarget2D>(SenderComponent, NAME_None);
            if(RenderTarget == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: failed to create render target"));
                return true;
            }
            RenderTarget->RenderTargetFormat = RTF_RGBA8;
            RenderTarget->ClearColor = FLinearColor::Black;
            RenderTarget->bAutoGenerateMips = false;
            RenderTarget->bForceLinearGamma = true;
            RenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
            RenderTarget->UpdateResourceImmediate(true);

            SenderActor->AddInstanceComponent(SenderComponent);
            SenderComponent->SenderName = Scenario.SenderName;
            SenderComponent->SourceRenderTarget = RenderTarget;
            SenderComponent->RegisterComponent();

            const bool bStarted = SenderComponent->StartSender();
            Test->AddInfo(FString::Printf(TEXT("NOZZLE_SMOKE_START started=%d diagnostics=%s"), bStarted ? 1 : 0, *NozzleSmokeDiagnosticsToString(SenderComponent->GetLastDiagnostics())));
            if(!bStarted)
            {
                Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: StartSender returned false"));
                return true;
            }
        }

        if(!DrawNozzleSmokePattern(RenderTarget, Scenario, PublishedFrames))
        {
            PatternUploadAttempts += 1;
            if(PatternUploadAttempts < 30)
            {
                return false;
            }
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: unable to enqueue render target pattern upload"));
            return true;
        }
        PatternUploadAttempts = 0;
        FlushRenderingCommands();
        const bool bQueued = SenderComponent->PublishFrame();
        FlushRenderingCommands();
        const FNozzleRuntimeDiagnostics RenderDiagnostics = SenderComponent->GetLastRenderDiagnostics();
        const int64 RenderSequence = SenderComponent->GetLastRenderSequence();

        if(PublishedFrames < 3 || (PublishedFrames % 30) == 0)
        {
            Test->AddInfo(FString::Printf(
                TEXT("NOZZLE_SMOKE_FRAME frame=%d queued=%d render_sequence=%lld diagnostics=%s"),
                PublishedFrames,
                bQueued ? 1 : 0,
                static_cast<long long>(RenderSequence),
                *NozzleSmokeDiagnosticsToString(RenderDiagnostics)));
        }

        if(!bQueued)
        {
            Test->AddError(FString::Printf(TEXT("NOZZLE_SMOKE_RESULT failed: PublishFrame returned false at frame=%d"), PublishedFrames));
            return true;
        }

        LastRenderSequence = RenderSequence;
        LastDiagnostics = RenderDiagnostics;
        PublishedFrames += 1;

        if(PublishedFrames < Scenario.FrameCount)
        {
            return false;
        }

        const bool bPublishedMultipleFrames = Scenario.FrameCount <= LastRenderSequence;
        const bool bHasExpectedSize = LastDiagnostics.Width == Scenario.Width && LastDiagnostics.Height == Scenario.Height;
        const bool bIOSurfaceBacked = LastDiagnostics.bIOSurfaceBacked && 0 < LastDiagnostics.IOSurfaceID;
        const bool bRenderDiagnosticsRunning = LastDiagnostics.State == ENozzleRuntimeState::Running && LastDiagnostics.bCanUseRuntime;
        const bool bPassCandidate = bPublishedMultipleFrames && bHasExpectedSize && bIOSurfaceBacked && bRenderDiagnosticsRunning;

        const TCHAR* RowStatus = bPassCandidate ? TEXT("PASS_CANDIDATE") : TEXT("MISSING");
        Test->AddInfo(FString::Printf(
            TEXT("NOZZLE_SMOKE_RESULT row_status=%s frames=%d last_sequence=%lld expected_size=%d iosurface_backed=%d iosurface_id=%llu render_running=%d strict=%d final=%s"),
            RowStatus,
            PublishedFrames,
            static_cast<long long>(LastRenderSequence),
            bHasExpectedSize ? 1 : 0,
            bIOSurfaceBacked ? 1 : 0,
            static_cast<unsigned long long>(LastDiagnostics.IOSurfaceID),
            bRenderDiagnosticsRunning ? 1 : 0,
            bRequireStrictPass ? 1 : 0,
            *NozzleSmokeDiagnosticsToString(LastDiagnostics)));

        if(!bPublishedMultipleFrames)
        {
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: render-thread sequence did not advance for every requested frame"));
        }
        if(!bHasExpectedSize)
        {
            Test->AddError(FString::Printf(TEXT("NOZZLE_SMOKE_RESULT failed: final diagnostics did not report %dx%d"), Scenario.Width, Scenario.Height));
        }
        if(!bIOSurfaceBacked && bRequireStrictPass)
        {
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: Metal texture was not reported as IOSurface-backed"));
        }
        if(!bRenderDiagnosticsRunning && bRequireStrictPass)
        {
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: final render diagnostics were not running"));
        }

        return true;
    }

private:
    FAutomationTestBase* Test = nullptr;
    FNozzleSmokeScenario Scenario;
    bool bRequireStrictPass = false;
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
    TObjectPtr<AActor> SenderActor = nullptr;
    TObjectPtr<UNozzleSenderComponent> SenderComponent = nullptr;
    FNozzleRuntimeDiagnostics LastDiagnostics;
    int64 LastRenderSequence = 0;
    int32 Attempts = 0;
    int32 PatternUploadAttempts = 0;
    int32 PublishedFrames = 0;
};


UMaterial* CreateNozzleSmokeReceiverMaterial(UObject* Outer)
{
    UMaterial* Material = NewObject<UMaterial>(Outer, TEXT("NozzleSmokeReceiverMaterial"), RF_Transient);
    if(Material == nullptr)
    {
        return nullptr;
    }

    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Translucent;
    Material->SetShadingModel(MSM_Unlit);
    Material->bUsedWithEditorCompositing = true;

    UMaterialExpressionTextureSampleParameter2D* TextureSample = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
    if(TextureSample == nullptr)
    {
        return nullptr;
    }
    TextureSample->ParameterName = TEXT("ReceiverTexture");
    TextureSample->SamplerType = SAMPLERTYPE_Color;
    Material->GetExpressionCollection().AddExpression(TextureSample);

#if WITH_EDITORONLY_DATA
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if(EditorOnlyData == nullptr)
    {
        return nullptr;
    }

    EditorOnlyData->BaseColor.Expression = TextureSample;
    EditorOnlyData->BaseColor.OutputIndex = 0;
    EditorOnlyData->EmissiveColor.Expression = TextureSample;
    EditorOnlyData->EmissiveColor.OutputIndex = 0;
    EditorOnlyData->Opacity.Expression = TextureSample;
    EditorOnlyData->Opacity.OutputIndex = 4;
#endif
    Material->PostEditChange();
    Material->ForceRecompileForRendering();
    return Material;
}

class FNozzleSmokeReceiveLatentCommand final : public IAutomationLatentCommand
{
public:
    FNozzleSmokeReceiveLatentCommand(FAutomationTestBase* InTest, const FNozzleSmokeScenario& InScenario, bool bInRequireStrictPass)
    : Test(InTest)
    , Scenario(InScenario)
    , bRequireStrictPass(bInRequireStrictPass)
    {}

    virtual bool Update() override
    {
        if(Test == nullptr)
        {
            return true;
        }

        UWorld* PIEWorld = FindNozzleSmokePIEWorld();
        if(PIEWorld == nullptr)
        {
            Attempts += 1;
            if(Attempts < 120)
            {
                return false;
            }
            Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: PIE world was not available"));
            return true;
        }

        if(RenderTarget == nullptr)
        {
            ReceiverActor = PIEWorld->SpawnActor<AActor>();
            if(ReceiverActor == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: SpawnActor returned null"));
                return true;
            }

            ReceiverComponent = NewObject<UNozzleReceiverComponent>(ReceiverActor, TEXT("NozzleSmokeReceiverComponent"));
            if(ReceiverComponent == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: failed to create receiver component"));
                return true;
            }

            RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("NozzleSmokeReceiverRenderTarget"), RF_Transient);
            if(RenderTarget == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: failed to create target render target"));
                return true;
            }
            RenderTarget->AddToRoot();
            RenderTarget->RenderTargetFormat = RTF_RGBA8;
            RenderTarget->ClearColor = FLinearColor::Black;
            RenderTarget->bAutoGenerateMips = false;
            RenderTarget->bForceLinearGamma = true;
            RenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
            RenderTarget->UpdateResourceImmediate(true);

            MaterialRenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), TEXT("NozzleSmokeMaterialRenderTarget"), RF_Transient);
            if(MaterialRenderTarget == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: failed to create material render target"));
                return true;
            }
            MaterialRenderTarget->AddToRoot();
            MaterialRenderTarget->RenderTargetFormat = RTF_RGBA8;
            MaterialRenderTarget->ClearColor = FLinearColor::Black;
            MaterialRenderTarget->bAutoGenerateMips = false;
            MaterialRenderTarget->bForceLinearGamma = true;
            MaterialRenderTarget->InitCustomFormat(Scenario.Width, Scenario.Height, PF_B8G8R8A8, false);
            MaterialRenderTarget->UpdateResourceImmediate(true);

            ReceiverMaterial = CreateNozzleSmokeReceiverMaterial(GetTransientPackage());
            if(ReceiverMaterial == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: failed to create receiver material"));
                return true;
            }
            ReceiverMaterial->AddToRoot();
            ReceiverMaterialInstance = UMaterialInstanceDynamic::Create(ReceiverMaterial, GetTransientPackage());
            if(ReceiverMaterialInstance == nullptr)
            {
                Test->AddError(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: failed to create receiver material instance"));
                return true;
            }
            ReceiverMaterialInstance->AddToRoot();
            ReceiverMaterialInstance->SetTextureParameterValue(TEXT("ReceiverTexture"), RenderTarget);

            ReceiverActor->AddInstanceComponent(ReceiverComponent);
            ReceiverComponent->SenderName = Scenario.SenderName;
            ReceiverComponent->TargetRenderTarget = RenderTarget;
            ReceiverComponent->AcquireTimeoutMs = 100;
            ReceiverComponent->RegisterComponent();
            Test->AddInfo(FString::Printf(TEXT("NOZZLE_RECEIVER_SMOKE_START source='%s' width=%d height=%d diagnostics=%s"), *Scenario.SenderName, Scenario.Width, Scenario.Height, *NozzleSmokeDiagnosticsToString(ReceiverComponent->GetLastDiagnostics())));
        }

        const bool bPolled = ReceiverComponent->PollFrame();
        FlushRenderingCommands();
        const FNozzleRuntimeDiagnostics RenderDiagnostics = ReceiverComponent->GetLastRenderDiagnostics();
        const int64 RenderSequence = ReceiverComponent->GetLastRenderSequence();
        if(RenderSequence != LastRenderSequence)
        {
            DistinctRenderSequences += 1;
            LastRenderSequence = RenderSequence;
            LastDiagnostics = RenderDiagnostics;
        }

        bool bMaterialDraw = false;
        if(ReceiverMaterialInstance != nullptr && MaterialRenderTarget != nullptr)
        {
            UKismetRenderingLibrary::DrawMaterialToRenderTarget(PIEWorld, MaterialRenderTarget, ReceiverMaterialInstance);
            FlushRenderingCommands();
            bMaterialDraw = true;
        }

        TArray<FColor> Pixels;
        FTextureRenderTargetResource* MaterialRenderTargetResource = MaterialRenderTarget != nullptr ? MaterialRenderTarget->GameThread_GetRenderTargetResource() : nullptr;
        const bool bReadPixels = MaterialRenderTargetResource != nullptr && MaterialRenderTargetResource->ReadPixels(Pixels);
        TArray<FColor> DirectPixels;
        FTextureRenderTargetResource* DirectRenderTargetResource = RenderTarget != nullptr ? RenderTarget->GameThread_GetRenderTargetResource() : nullptr;
        const bool bReadDirectPixels = DirectRenderTargetResource != nullptr && DirectRenderTargetResource->ReadPixels(DirectPixels);
        TArray<FNozzleSmokeReceiverSample> Samples;
        TArray<FNozzleSmokeReceiverSample> DirectSamples;
        bool bMaterialPatternOk = false;
        bool bDirectPatternOk = false;
        bool bMarkerFound = false;
        int32 MarkerX = -1;
        if(bReadPixels)
        {
            const int32 LeftX = Scenario.Width / 8;
            const int32 RightX = Scenario.Width - 1 - (Scenario.Width / 8);
            const int32 TopY = Scenario.Height / 8;
            const int32 BottomY = Scenario.Height - 1 - (Scenario.Height / 8);
            const int32 AlphaX = Scenario.Width / 2;
            const int32 AlphaY = (Scenario.Height / 2) - (Scenario.Height / 16);
            Samples.SetNum(5);
            const bool bTopLeftRed = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, TEXT("material_top_left_red_rgb"), LeftX, TopY, FColor(255, 0, 0, 255), Samples[0], false);
            const bool bTopRightGreen = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, TEXT("material_top_right_green_rgb"), RightX, TopY, FColor(0, 255, 0, 255), Samples[1], false);
            const bool bBottomLeftBlue = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, TEXT("material_bottom_left_blue_rgb"), LeftX, BottomY, FColor(0, 0, 255, 255), Samples[2], false);
            const bool bBottomRightWhite = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, TEXT("material_bottom_right_white_rgb"), RightX, BottomY, FColor(255, 255, 255, 255), Samples[3], false);
            const bool bAlphaPatchRgb = NozzleSmokeSampleColor(Pixels, Scenario.Width, Scenario.Height, TEXT("material_center_magenta_alpha_patch_rgb"), AlphaX, AlphaY, FColor(255, 0, 255, 64), Samples[4], false);
            bMaterialPatternOk = bTopLeftRed && bTopRightGreen && bBottomLeftBlue && bBottomRightWhite && bAlphaPatchRgb;
            bMarkerFound = NozzleSmokeFindYellowMarkerX(Pixels, Scenario.Width, Scenario.Height, MarkerX, false);
            if(bMarkerFound)
            {
                MarkerXs.Add(MarkerX);
            }
        }
        if(bReadDirectPixels)
        {
            const int32 LeftX = Scenario.Width / 8;
            const int32 RightX = Scenario.Width - 1 - (Scenario.Width / 8);
            const int32 TopY = Scenario.Height / 8;
            const int32 BottomY = Scenario.Height - 1 - (Scenario.Height / 8);
            const int32 AlphaX = Scenario.Width / 2;
            const int32 AlphaY = (Scenario.Height / 2) - (Scenario.Height / 16);
            DirectSamples.SetNum(5);
            const bool bDirectTopLeftRed = NozzleSmokeSampleColor(DirectPixels, Scenario.Width, Scenario.Height, TEXT("direct_top_left_red_rgba"), LeftX, TopY, FColor(255, 0, 0, 255), DirectSamples[0]);
            const bool bDirectTopRightGreen = NozzleSmokeSampleColor(DirectPixels, Scenario.Width, Scenario.Height, TEXT("direct_top_right_green_rgba"), RightX, TopY, FColor(0, 255, 0, 255), DirectSamples[1]);
            const bool bDirectBottomLeftBlue = NozzleSmokeSampleColor(DirectPixels, Scenario.Width, Scenario.Height, TEXT("direct_bottom_left_blue_rgba"), LeftX, BottomY, FColor(0, 0, 255, 255), DirectSamples[2]);
            const bool bDirectBottomRightWhite = NozzleSmokeSampleColor(DirectPixels, Scenario.Width, Scenario.Height, TEXT("direct_bottom_right_white_rgba"), RightX, BottomY, FColor(255, 255, 255, 255), DirectSamples[3]);
            const bool bDirectAlphaPatch = NozzleSmokeSampleColor(DirectPixels, Scenario.Width, Scenario.Height, TEXT("direct_center_magenta_alpha_patch_rgba"), AlphaX, AlphaY, FColor(255, 0, 255, 64), DirectSamples[4]);
            bDirectPatternOk = bDirectTopLeftRed && bDirectTopRightGreen && bDirectBottomLeftBlue && bDirectBottomRightWhite && bDirectAlphaPatch;
        }

        if(ObservedFrames < 3 || (ObservedFrames % 30) == 0)
        {
            Test->AddInfo(FString::Printf(
                TEXT("NOZZLE_RECEIVER_SMOKE_FRAME frame=%d polled=%d material_draw=%d read_pixels=%d read_direct_pixels=%d render_sequence=%lld material_pattern_ok=%d direct_pattern_ok=%d marker_found=%d marker_x=%d diagnostics=%s material_samples=%s direct_samples=%s"),
                ObservedFrames,
                bPolled ? 1 : 0,
                bMaterialDraw ? 1 : 0,
                bReadPixels ? 1 : 0,
                bReadDirectPixels ? 1 : 0,
                static_cast<long long>(RenderSequence),
                bMaterialPatternOk ? 1 : 0,
                bDirectPatternOk ? 1 : 0,
                bMarkerFound ? 1 : 0,
                MarkerX,
                *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
                *NozzleSmokeSamplesToString(Samples),
                *NozzleSmokeSamplesToString(DirectSamples)));
        }
        ObservedFrames += 1;

        const bool bExpectedSize = RenderDiagnostics.Width == Scenario.Width && RenderDiagnostics.Height == Scenario.Height;
        const bool bRenderDiagnosticsRunning = RenderDiagnostics.State == ENozzleRuntimeState::Running && RenderDiagnostics.bCanUseRuntime;
        const bool bMetalRHI = RenderDiagnostics.bMetalRHI && RenderDiagnostics.Backend == TEXT("Metal");
        const bool bNativeDetailsOk = RenderDiagnostics.NativeTextureDetails.Contains(TEXT("receiver_target=FRHITexture::GetNativeResource"))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("MTLPixelFormat=81"))
            && RenderDiagnostics.NativeTextureDetails.Contains(FString::Printf(TEXT("width=%d"), Scenario.Width))
            && RenderDiagnostics.NativeTextureDetails.Contains(FString::Printf(TEXT("height=%d"), Scenario.Height))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("storageMode="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("usage="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("iosurface_backed="))
            && RenderDiagnostics.NativeTextureDetails.Contains(TEXT("iosurface_id="));
        const bool bTransferPathNamed = RenderDiagnostics.TransferMode == TEXT("nozzle_frame_to_unreal_metal_texture") && bNativeDetailsOk;
        const bool bSyncBoundaryNamed = RenderDiagnostics.SynchronizationBoundary.Contains(TEXT("nozzle_frame_copy_to_native_texture"))
            && RenderDiagnostics.SynchronizationBoundary.Contains(TEXT("backend Metal copy wait completes"));
        const bool bMarkerMoved = 1 < MarkerXs.Num();
        const bool bEnoughFrames = 6 <= DistinctRenderSequences;
        const bool bPassCandidate = bExpectedSize && bRenderDiagnosticsRunning && bMetalRHI && bTransferPathNamed && bSyncBoundaryNamed && bMaterialDraw && bReadPixels && bReadDirectPixels && bMaterialPatternOk && bDirectPatternOk && bMarkerFound && bMarkerMoved && bEnoughFrames;
        if(bPassCandidate)
        {
            Test->AddInfo(FString::Printf(
                TEXT("NOZZLE_RECEIVER_SMOKE_RESULT row_status=PASS_CANDIDATE source='%s' frames=%d distinct_sequences=%d marker_positions=%d last_marker_x=%d expected_size=%d metal=%d material_draw=%d material_pattern_ok=%d direct_rgba_ok=%d transfer_path=%d native_details=%d sync=%d strict=%d final=%s material_samples=%s direct_samples=%s"),
                *Scenario.SenderName,
                ObservedFrames,
                DistinctRenderSequences,
                MarkerXs.Num(),
                MarkerX,
                bExpectedSize ? 1 : 0,
                bMetalRHI ? 1 : 0,
                bMaterialDraw ? 1 : 0,
                bMaterialPatternOk ? 1 : 0,
                bDirectPatternOk ? 1 : 0,
                bTransferPathNamed ? 1 : 0,
                bNativeDetailsOk ? 1 : 0,
                bSyncBoundaryNamed ? 1 : 0,
                bRequireStrictPass ? 1 : 0,
                *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
                *NozzleSmokeSamplesToString(Samples),
                *NozzleSmokeSamplesToString(DirectSamples)));
            CleanupRootedObjects();
            return true;
        }

        const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
        if(ElapsedSeconds < 90.0)
        {
            return false;
        }

        Test->AddError(FString::Printf(
            TEXT("NOZZLE_RECEIVER_SMOKE_RESULT failed: timeout source='%s' frames=%d distinct_sequences=%d marker_positions=%d last_marker_x=%d expected_size=%d material_draw=%d read_pixels=%d read_direct_pixels=%d material_pattern_ok=%d direct_rgba_ok=%d marker_found=%d metal=%d transfer_path=%d native_details=%d sync=%d final=%s material_samples=%s direct_samples=%s"),
            *Scenario.SenderName,
            ObservedFrames,
            DistinctRenderSequences,
            MarkerXs.Num(),
            MarkerX,
            bExpectedSize ? 1 : 0,
            bMaterialDraw ? 1 : 0,
            bReadPixels ? 1 : 0,
            bReadDirectPixels ? 1 : 0,
            bMaterialPatternOk ? 1 : 0,
            bDirectPatternOk ? 1 : 0,
            bMarkerFound ? 1 : 0,
            bMetalRHI ? 1 : 0,
            bTransferPathNamed ? 1 : 0,
            bNativeDetailsOk ? 1 : 0,
            bSyncBoundaryNamed ? 1 : 0,
            *NozzleSmokeDiagnosticsToString(RenderDiagnostics),
            *NozzleSmokeSamplesToString(Samples),
            *NozzleSmokeSamplesToString(DirectSamples)));
        CleanupRootedObjects();
        return true;
    }

private:
    void CleanupRootedObjects()
    {
        if(ReceiverMaterialInstance != nullptr && ReceiverMaterialInstance->IsRooted())
        {
            ReceiverMaterialInstance->RemoveFromRoot();
        }
        if(ReceiverMaterial != nullptr && ReceiverMaterial->IsRooted())
        {
            ReceiverMaterial->RemoveFromRoot();
        }
        if(MaterialRenderTarget != nullptr && MaterialRenderTarget->IsRooted())
        {
            MaterialRenderTarget->RemoveFromRoot();
        }
        if(RenderTarget != nullptr && RenderTarget->IsRooted())
        {
            RenderTarget->RemoveFromRoot();
        }
    }

    FAutomationTestBase* Test = nullptr;
    FNozzleSmokeScenario Scenario;
    bool bRequireStrictPass = false;
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
    TObjectPtr<UTextureRenderTarget2D> MaterialRenderTarget = nullptr;
    TObjectPtr<UMaterial> ReceiverMaterial = nullptr;
    TObjectPtr<UMaterialInstanceDynamic> ReceiverMaterialInstance = nullptr;
    TObjectPtr<AActor> ReceiverActor = nullptr;
    TObjectPtr<UNozzleReceiverComponent> ReceiverComponent = nullptr;
    FNozzleRuntimeDiagnostics LastDiagnostics;
    int64 LastRenderSequence = 0;
    int32 Attempts = 0;
    int32 ObservedFrames = 0;
    int32 DistinctRenderSequences = 0;
    TSet<int32> MarkerXs;
    double StartSeconds = FPlatformTime::Seconds();
};

bool RunNozzleSmokeUnrealSenderToViewerMacMetalTest(FAutomationTestBase& Test, const FNozzleSmokeScenario& Scenario)
{
    if(!PLATFORM_MAC)
    {
        Test.AddInfo(TEXT("NOZZLE_SMOKE_RESULT skipped: Mac Metal diagnostic is not runnable on this platform"));
        return true;
    }

    const bool bRequireStrictPass = FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokeStrictPass"));
    FAutomationEditorCommonUtils::CreateNewMap();
    Test.AddCommand(new FStartPIECommand(false));
    Test.AddCommand(new FNozzleSmokePublishLatentCommand(&Test, Scenario, bRequireStrictPass));
    Test.AddCommand(new FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNozzleSmokeUnrealSenderToViewerMacMetal320Test,
    "Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNozzleSmokeUnrealSenderToViewerMacMetal320Test::RunTest(const FString& Parameters)
{
    FNozzleSmokeScenario Scenario;
    Scenario.Width = 320;
    Scenario.Height = 240;
    Scenario.SenderName = TEXT("NozzleUnrealSmoke320");
    return RunNozzleSmokeUnrealSenderToViewerMacMetalTest(*this, Scenario);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNozzleSmokeUnrealSenderToViewerMacMetal641Test,
    "Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.641x479",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNozzleSmokeUnrealSenderToViewerMacMetal641Test::RunTest(const FString& Parameters)
{
    FNozzleSmokeScenario Scenario;
    Scenario.Width = 641;
    Scenario.Height = 479;
    Scenario.SenderName = TEXT("NozzleUnrealSmoke641");
    return RunNozzleSmokeUnrealSenderToViewerMacMetalTest(*this, Scenario);
}


bool RunNozzleSmokeViewerToUnrealReceiverMacMetalTest(FAutomationTestBase& Test, const FNozzleSmokeScenario& Scenario)
{
    if(!PLATFORM_MAC)
    {
        Test.AddInfo(TEXT("NOZZLE_RECEIVER_SMOKE_RESULT skipped: Mac Metal diagnostic is not runnable on this platform"));
        return true;
    }

    const bool bRequireStrictPass = FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokeStrictPass"));
    FAutomationEditorCommonUtils::CreateNewMap();
    Test.AddCommand(new FStartPIECommand(false));
    Test.AddCommand(new FNozzleSmokeReceiveLatentCommand(&Test, Scenario, bRequireStrictPass));
    Test.AddCommand(new FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNozzleSmokeViewerToUnrealReceiverMacMetal320Test,
    "Nozzle.Smoke.MacMetal.ViewerToUnrealReceiver.EditorPIE.320x240",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNozzleSmokeViewerToUnrealReceiverMacMetal320Test::RunTest(const FString& Parameters)
{
    FNozzleSmokeScenario Scenario;
    Scenario.Width = 320;
    Scenario.Height = 240;
    Scenario.SenderName = TEXT("NozzleViewerSmoke320");
    return RunNozzleSmokeViewerToUnrealReceiverMacMetalTest(*this, Scenario);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNozzleSmokeViewerToUnrealReceiverMacMetal641Test,
    "Nozzle.Smoke.MacMetal.ViewerToUnrealReceiver.EditorPIE.641x479",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNozzleSmokeViewerToUnrealReceiverMacMetal641Test::RunTest(const FString& Parameters)
{
    FNozzleSmokeScenario Scenario;
    Scenario.Width = 641;
    Scenario.Height = 479;
    Scenario.SenderName = TEXT("NozzleViewerSmoke641");
    return RunNozzleSmokeViewerToUnrealReceiverMacMetalTest(*this, Scenario);
}

#endif

class FNozzleSmokeModule final : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();

        if(FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokePackagedSender")))
        {
            FNozzleSmokeScenario Scenario;
            FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeWidth="), Scenario.Width);
            FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeHeight="), Scenario.Height);
            FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeFrameCount="), Scenario.FrameCount);
            Scenario.FrameCount = FMath::Max(1, Scenario.FrameCount);
            FString SenderName;
            if(FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeSource="), SenderName) && !SenderName.IsEmpty())
            {
                Scenario.SenderName = SenderName;
            }
            else if(Scenario.Width == 641 && Scenario.Height == 479)
            {
                Scenario.SenderName = TEXT("NozzleUnrealSmoke641");
            }
            else
            {
                Scenario.SenderName = TEXT("NozzleUnrealSmoke320");
            }

            const bool bRequireStrictPass = FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokeStrictPass"));
            SenderRuntimeRunner = MakeUnique<FNozzleSmokeRuntimeRunner>(Scenario, bRequireStrictPass);
            RuntimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNozzleSmokeModule::TickRuntimeSmoke));
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_CONFIG packaged=1 width=%d height=%d frames=%d source='%s' strict=%d"), Scenario.Width, Scenario.Height, Scenario.FrameCount, *Scenario.SenderName, bRequireStrictPass ? 1 : 0);
        }
        else if(FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokePackagedReceiver")))
        {
            FNozzleSmokeScenario Scenario;
            FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeWidth="), Scenario.Width);
            FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeHeight="), Scenario.Height);
            FString SenderName;
            if(Scenario.Width < NozzleSmokeMarkerWidth || Scenario.Height <= NozzleSmokeMarkerY + NozzleSmokeMarkerHeight)
            {
                UE_LOG(LogNozzleSmoke, Error, TEXT("NOZZLE_RECEIVER_SMOKE_CONFIG packaged=1 invalid_size width=%d height=%d min_width=%d min_height=%d"), Scenario.Width, Scenario.Height, NozzleSmokeMarkerWidth, NozzleSmokeMarkerY + NozzleSmokeMarkerHeight + 1);
                FPlatformMisc::RequestExitWithStatus(false, EXIT_FAILURE);
                std::exit(EXIT_FAILURE);
            }

            if(FParse::Value(FCommandLine::Get(), TEXT("NozzleSmokeSource="), SenderName) && !SenderName.IsEmpty())
            {
                Scenario.SenderName = SenderName;
            }
            else if(Scenario.Width == 641 && Scenario.Height == 479)
            {
                Scenario.SenderName = TEXT("NozzleViewerSmoke641");
            }
            else
            {
                Scenario.SenderName = TEXT("NozzleViewerSmoke320");
            }

            const bool bRequireStrictPass = FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokeStrictPass"));
            ReceiverRuntimeRunner = MakeUnique<FNozzleSmokePackagedReceiveRunner>(Scenario, bRequireStrictPass);
            RuntimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNozzleSmokeModule::TickRuntimeSmoke));
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_RECEIVER_SMOKE_CONFIG packaged=1 width=%d height=%d source='%s' strict=%d"), Scenario.Width, Scenario.Height, *Scenario.SenderName, bRequireStrictPass ? 1 : 0);
        }
    }

    virtual void ShutdownModule() override
    {
        if(RuntimeTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(RuntimeTickerHandle);
            RuntimeTickerHandle.Reset();
        }
        SenderRuntimeRunner.Reset();
        ReceiverRuntimeRunner.Reset();
        FDefaultGameModuleImpl::ShutdownModule();
    }

private:
    bool TickRuntimeSmoke(float DeltaTime)
    {
        (void)DeltaTime;
        if(SenderRuntimeRunner.IsValid())
        {
            return SenderRuntimeRunner->Tick();
        }
        if(ReceiverRuntimeRunner.IsValid())
        {
            return ReceiverRuntimeRunner->Tick();
        }
        return false;
    }

    TUniquePtr<FNozzleSmokeRuntimeRunner> SenderRuntimeRunner;
    TUniquePtr<FNozzleSmokePackagedReceiveRunner> ReceiverRuntimeRunner;
    FTSTicker::FDelegateHandle RuntimeTickerHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FNozzleSmokeModule, NozzleSmoke, "NozzleSmoke");
