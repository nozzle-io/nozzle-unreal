#include "NozzleSmoke.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "NozzleDiagnostics.h"
#include "NozzleSenderComponent.h"
#include "RenderingThread.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#endif

#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, NozzleSmoke, "NozzleSmoke");

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{

constexpr int32 NozzleSmokeWidth = 320;
constexpr int32 NozzleSmokeHeight = 240;
constexpr int32 NozzleSmokeFrameCount = 180;

FString NozzleSmokeDiagnosticsToString(const FNozzleRuntimeDiagnostics& Diagnostics)
{
    return FString::Printf(
        TEXT("state=%d can_use_runtime=%d backend='%s' width=%d height=%d message='%s' native='%s' sync='%s' iosurface_backed=%d iosurface_id=%llu"),
        static_cast<int32>(Diagnostics.State),
        Diagnostics.bCanUseRuntime ? 1 : 0,
        *Diagnostics.Backend,
        Diagnostics.Width,
        Diagnostics.Height,
        *Diagnostics.Message,
        *Diagnostics.NativeTextureDetails,
        *Diagnostics.SynchronizationBoundary,
        Diagnostics.bIOSurfaceBacked ? 1 : 0,
        static_cast<unsigned long long>(Diagnostics.IOSurfaceID));
}

UWorld* FindNozzleSmokePIEWorld()
{
    if(GEditor != nullptr && GEditor->PlayWorld != nullptr)
    {
        return GEditor->PlayWorld;
    }

    if(GEngine == nullptr)
    {
        return nullptr;
    }

    UWorld* FoundPIEWorld = nullptr;
    for(const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
        if(WorldContext.WorldType == EWorldType::PIE && WorldContext.World() != nullptr)
        {
            if(FoundPIEWorld != nullptr)
            {
                return nullptr;
            }
            FoundPIEWorld = WorldContext.World();
        }
    }

    return FoundPIEWorld;
}

void DrawNozzleSmokePattern(UWorld* World, UTextureRenderTarget2D* RenderTarget, int32 FrameIndex)
{
    if(World == nullptr || RenderTarget == nullptr)
    {
        return;
    }

    FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
    if(RenderTargetResource == nullptr)
    {
        return;
    }

    FCanvas Canvas(RenderTargetResource, nullptr, World, World->GetFeatureLevel());
    Canvas.Clear(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));

    FCanvasTileItem TopLeft(FVector2D(0.0f, 0.0f), FVector2D(96.0f, 72.0f), FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
    TopLeft.BlendMode = SE_BLEND_Opaque;
    Canvas.DrawItem(TopLeft);

    FCanvasTileItem TopRight(FVector2D(224.0f, 0.0f), FVector2D(96.0f, 72.0f), FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
    TopRight.BlendMode = SE_BLEND_Opaque;
    Canvas.DrawItem(TopRight);

    FCanvasTileItem BottomLeft(FVector2D(0.0f, 168.0f), FVector2D(96.0f, 72.0f), FLinearColor(0.0f, 0.0f, 1.0f, 1.0f));
    BottomLeft.BlendMode = SE_BLEND_Opaque;
    Canvas.DrawItem(BottomLeft);

    FCanvasTileItem BottomRight(FVector2D(224.0f, 168.0f), FVector2D(96.0f, 72.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    BottomRight.BlendMode = SE_BLEND_Opaque;
    Canvas.DrawItem(BottomRight);

    const float MarkerX = static_cast<float>((FrameIndex * 29) % (NozzleSmokeWidth - 24));
    FCanvasTileItem FrameMarker(FVector2D(MarkerX, 104.0f), FVector2D(24.0f, 32.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    FrameMarker.BlendMode = SE_BLEND_Opaque;
    Canvas.DrawItem(FrameMarker);

    Canvas.Flush_GameThread();
}

class FNozzleSmokePublishLatentCommand final : public IAutomationLatentCommand
{
public:
    FNozzleSmokePublishLatentCommand(FAutomationTestBase* InTest, bool bInRequireStrictPass)
    : Test(InTest)
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
            RenderTarget->InitCustomFormat(NozzleSmokeWidth, NozzleSmokeHeight, PF_B8G8R8A8, false);
            RenderTarget->UpdateResourceImmediate(true);

            SenderActor->AddInstanceComponent(SenderComponent);
            SenderComponent->SenderName = TEXT("NozzleUnrealSmoke320");
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

        DrawNozzleSmokePattern(PIEWorld, RenderTarget, PublishedFrames);
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

        if(PublishedFrames < NozzleSmokeFrameCount)
        {
            return false;
        }

        const bool bPublishedMultipleFrames = NozzleSmokeFrameCount <= LastRenderSequence;
        const bool bHasExpectedSize = LastDiagnostics.Width == NozzleSmokeWidth && LastDiagnostics.Height == NozzleSmokeHeight;
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
            Test->AddError(TEXT("NOZZLE_SMOKE_RESULT failed: final diagnostics did not report 320x240"));
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
    bool bRequireStrictPass = false;
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
    TObjectPtr<AActor> SenderActor = nullptr;
    TObjectPtr<UNozzleSenderComponent> SenderComponent = nullptr;
    FNozzleRuntimeDiagnostics LastDiagnostics;
    int64 LastRenderSequence = 0;
    int32 Attempts = 0;
    int32 PublishedFrames = 0;
};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNozzleSmokeUnrealSenderToViewerMacMetalTest,
    "Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNozzleSmokeUnrealSenderToViewerMacMetalTest::RunTest(const FString& Parameters)
{
    if(!PLATFORM_MAC)
    {
        AddInfo(TEXT("NOZZLE_SMOKE_RESULT skipped: Mac Metal diagnostic is not runnable on this platform"));
        return true;
    }

    const bool bRequireStrictPass = FParse::Param(FCommandLine::Get(), TEXT("NozzleSmokeStrictPass"));
    FAutomationEditorCommonUtils::CreateNewMap();
    AddCommand(new FStartPIECommand(false));
    AddCommand(new FNozzleSmokePublishLatentCommand(this, bRequireStrictPass));
    AddCommand(new FEndPlayMapCommand());
    return true;
}
#endif
