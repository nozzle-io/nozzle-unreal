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
#include "NozzleSenderComponent.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

#include <cstdlib>

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
            Attempts += 1;
            if(Attempts < 120)
            {
                return true;
            }
            Finish(false, TEXT("game world was not available"));
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
    int32 Attempts = 0;
    int32 PatternUploadAttempts = 0;
    int32 PublishedFrames = 0;
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
            RuntimeRunner = MakeUnique<FNozzleSmokeRuntimeRunner>(Scenario, bRequireStrictPass);
            RuntimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNozzleSmokeModule::TickRuntimeSmoke));
            UE_LOG(LogNozzleSmoke, Display, TEXT("NOZZLE_SMOKE_CONFIG packaged=1 width=%d height=%d frames=%d source='%s' strict=%d"), Scenario.Width, Scenario.Height, Scenario.FrameCount, *Scenario.SenderName, bRequireStrictPass ? 1 : 0);
        }
    }

    virtual void ShutdownModule() override
    {
        if(RuntimeTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(RuntimeTickerHandle);
            RuntimeTickerHandle.Reset();
        }
        RuntimeRunner.Reset();
        FDefaultGameModuleImpl::ShutdownModule();
    }

private:
    bool TickRuntimeSmoke(float DeltaTime)
    {
        (void)DeltaTime;
        if(!RuntimeRunner.IsValid())
        {
            return false;
        }
        return RuntimeRunner->Tick();
    }

    TUniquePtr<FNozzleSmokeRuntimeRunner> RuntimeRunner;
    FTSTicker::FDelegateHandle RuntimeTickerHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FNozzleSmokeModule, NozzleSmoke, "NozzleSmoke");
