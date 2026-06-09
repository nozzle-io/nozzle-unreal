#include "NozzleSmoke.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "NozzleDiagnostics.h"
#include "NozzleSenderComponent.h"
#include "RHICommandList.h"
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

void FillNozzleSmokeRect(TArray<uint8>& Pixels, int32 X, int32 Y, int32 Width, int32 Height, uint8 Red, uint8 Green, uint8 Blue, uint8 Alpha)
{
    for(int32 Row = Y; Row < Y + Height; Row++)
    {
        for(int32 Column = X; Column < X + Width; Column++)
        {
            const int32 Offset = ((Row * NozzleSmokeWidth) + Column) * 4;
            Pixels[Offset + 0] = Blue;
            Pixels[Offset + 1] = Green;
            Pixels[Offset + 2] = Red;
            Pixels[Offset + 3] = Alpha;
        }
    }
}

bool DrawNozzleSmokePattern(UTextureRenderTarget2D* RenderTarget, int32 FrameIndex)
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
    Pixels->SetNumZeroed(NozzleSmokeWidth * NozzleSmokeHeight * 4);
    for(int32 Index = 0; Index < NozzleSmokeWidth * NozzleSmokeHeight; Index++)
    {
        const int32 Offset = Index * 4;
        (*Pixels)[Offset + 0] = 5;
        (*Pixels)[Offset + 1] = 5;
        (*Pixels)[Offset + 2] = 5;
        (*Pixels)[Offset + 3] = 255;
    }

    FillNozzleSmokeRect(*Pixels, 0, 0, 96, 72, 255, 0, 0, 255);
    FillNozzleSmokeRect(*Pixels, 224, 0, 96, 72, 0, 255, 0, 255);
    FillNozzleSmokeRect(*Pixels, 0, 168, 96, 72, 0, 0, 255, 255);
    FillNozzleSmokeRect(*Pixels, 224, 168, 96, 72, 255, 255, 255, 255);
    FillNozzleSmokeRect(*Pixels, 136, 84, 48, 32, 255, 0, 255, 64);

    const int32 MarkerX = (FrameIndex * 29) % (NozzleSmokeWidth - 24);
    FillNozzleSmokeRect(*Pixels, MarkerX, 128, 24, 32, 255, 255, 0, 255);

    FTextureRHIRef TextureRHI = RenderTargetResource->GetRenderTargetTexture();
    if(!TextureRHI.IsValid())
    {
        return false;
    }

    ENQUEUE_RENDER_COMMAND(NozzleSmokeUpdateRenderTarget)(
        [TextureRHI, Pixels](FRHICommandListImmediate& RHICmdList)
        {
            const FUpdateTextureRegion2D Region(0, 0, 0, 0, NozzleSmokeWidth, NozzleSmokeHeight);
            RHICmdList.UpdateTexture2D(TextureRHI, 0, Region, NozzleSmokeWidth * 4, Pixels->GetData());
        });
    return true;
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
            RenderTarget->bForceLinearGamma = true;
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

        if(!DrawNozzleSmokePattern(RenderTarget, PublishedFrames))
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
    int32 PatternUploadAttempts = 0;
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
