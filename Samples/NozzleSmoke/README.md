# NozzleSmoke sample

Minimal Unreal sample project for nozzle runtime diagnostics. It contains
Editor PIE automation tests for the macOS Metal sender-to-viewer rows:

```text
Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240
Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.641x479
```

It also contains a packaged-game sender harness enabled by
`-NozzleSmokePackagedSender`. The packaged harness uses the same transient
render-target pattern and source names, then exits with status 0 only after the
requested frame sequence has been published. Use `-NozzleSmokeFrameCount=<n>`
to extend the sender lifetime when attaching an external receiver after launch.

Each path creates a transient `UTextureRenderTarget2D` at the row size, draws a
non-symmetric quadrant pattern with a deliberate alpha patch and a moving frame
marker, publishes it through `UNozzleSenderComponent`, and logs
`NOZZLE_SMOKE_*` diagnostics including native Metal texture details, IOSurface
state, transfer mode, synchronization text, dimensions, and multi-frame render
sequence.

## Running the diagnostic

1. Install or copy the `Nozzle` plugin into a location Unreal can discover. For
   local repository work, a temporary `Samples/NozzleSmoke/Plugins/Nozzle`
   symlink to `../../../Nozzle` is sufficient; do not commit that symlink.
2. Stage the platform-native nozzle payload when runtime probing is needed:

   ```sh
   python3 scripts/stage_native_nozzle.py --platform Mac
   python3 scripts/check_native_staging.py --require Mac --inspect-deps
   ```

3. Build the editor target with the tested Unreal version:

   ```sh
   '/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh' \
     NozzleSmokeEditor Mac Development \
     -Project='/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
     -NoHotReload
   ```

4. Run the Editor PIE diagnostic. Pick the automation row matching the
   receiver dimensions you will capture:

   ```sh
   '/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor' \
     '/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
     -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput \
     -ExecCmds='Automation RunTests Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240' \
     -TestExit='Automation Test Queue Empty'

   '/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor' \
     '/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
     -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput \
     -ExecCmds='Automation RunTests Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.641x479' \
     -TestExit='Automation Test Queue Empty'
   ```

Add `-NozzleSmokeStrictPass` when the run must fail unless the sender-side Metal
gate reaches `PASS_CANDIDATE`. This is still not a cross-process runtime PASS:
it only checks the sender diagnostics, IOSurface-backed publish texture, expected
size, and multi-frame render sequence. Without that flag, the automation logs
`row_status=MISSING` instead of failing when Unreal's texture is not
IOSurface-backed.

The macOS Metal sender path may publish through an IOSurface-backed intermediate
instead of the raw Unreal render target. A PASS candidate must name that transfer
mode, for example `unreal_metal_blit_to_iosurface`, and must show the texture
actually passed into nozzle is IOSurface-backed with a non-zero IOSurface ID.

5. While the Unreal sender diagnostic is running, capture receiver evidence with
   `nozzle-viewer` and save the JSON artifact. Match the source name and
   dimensions to the row under test:

   ```sh
   /path/to/nozzle-viewer \
     --smoke-receiver \
     --source NozzleUnrealSmoke320 \
     --width 320 \
     --height 240 \
     --min-frames 5 \
     --timeout-ms 120000 \
     --expect-alpha-patch \
     --expect-moving-marker \
     --evidence /tmp/nozzle-unreal-viewer-smoke-320.json

   /path/to/nozzle-viewer \
     --smoke-receiver \
     --source NozzleUnrealSmoke641 \
     --width 641 \
     --height 479 \
     --min-frames 5 \
     --timeout-ms 120000 \
     --expect-alpha-patch \
     --expect-moving-marker \
     --evidence /tmp/nozzle-unreal-viewer-smoke-641.json
   ```

The viewer evidence must report at least five distinct frame indices in
`frame.observed_indices`, per-frame moving marker samples in
`frame.observed_marker_samples`, `checks.moving_marker=PASS`, and
`checks.distinct_frames=PASS`. Marker evidence is valid only when at least five
distinct frames have PASS samples with changing `x` positions and each sample
records `expected_rgba`, `actual_rgba`, and `passed`. The alpha patch sample
must appear as RGBA `[255,0,255,64]` via `samples[].expected_rgba` /
`samples[].actual_rgba`; the expected sample coordinate is `x=160,y=105` for
320x240 and `x=320,y=210` for 641x479. A single correct-looking frame is not
freshness evidence.

## Evidence boundary

A successful automation process is **not runtime evidence** by itself. Treat the
row as PASS only when the issue evidence includes:

- exact Unreal Engine, `nozzle-unreal`, and nozzle core SHAs;
- Metal RHI confirmation;
- `NativeTextureDetails`, `TransferMode`, `bIOSurfaceBacked`, and `IOSurfaceID`
  for the texture actually passed into nozzle;
- concrete synchronization boundary/outcome;
- multi-frame marker / stale-frame evidence from the viewer JSON:
  `frame.observed_indices`, `frame.observed_marker_samples`,
  `checks.moving_marker`, and `checks.distinct_frames`;
- captured `nozzle-viewer` output proving dimensions, orientation, channel order,
  and alpha behavior.


6. For the packaged Development row, package the sample and run the app with the
   packaged sender harness. Keep the receiver command on the same source name
   (`NozzleUnrealSmoke320`) and dimensions (`320x240`) while the app is running.
   Do not add `-archive` unless you specifically need a copied archive; the
   self-contained app produced by `-package` is enough for this smoke row. The
   sample's macOS entitlements intentionally do not enable App Sandbox because
   sandboxed packaged apps cannot create the globally lookupable IOSurfaces used
   for cross-process nozzle sharing.

   ```sh
   '/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh' \
     BuildCookRun \
     -project='/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
     -noP4 -platform=Mac -clientconfig=Development -build -cook -stage -package

   '/path/to/nozzle-unreal/Samples/NozzleSmoke/Binaries/Mac/NozzleSmoke.app/Contents/MacOS/NozzleSmoke' \
     -NozzleSmokePackagedSender \
     -NozzleSmokeWidth=320 \
     -NozzleSmokeHeight=240 \
     -NozzleSmokeSource=NozzleUnrealSmoke320 \
     -NozzleSmokeFrameCount=1200 \
     -NozzleSmokeStrictPass \
     -stdout -FullStdOutLogOutput
   ```

   The packaged app must verify with `codesign --verify --deep --strict`, use a
   project-owned bundle identifier (`org.nozzle-io.unreal-smoke`), and have no
   `com.apple.security.app-sandbox` entitlement. The packaged log must include
   `NOZZLE_SMOKE_CONFIG packaged=1 width=320 height=240 frames=<n>`,
   `NOZZLE_SMOKE_RESULT packaged=1 row_status=PASS_CANDIDATE`, and
   `NOZZLE_SMOKE_EXIT packaged=1 success=1`. The row is still not PASS unless
   the concurrent `nozzle-viewer` JSON proves the captured frames.

If `row_status=MISSING` or the viewer reports failed quadrant semantics, keep the
runtime matrix row MISSING/FAIL and split the focused bug instead of claiming
Metal runtime support.

## Packaged receiver/material smoke

The sample also contains a packaged-game receiver/material harness enabled by
`-NozzleSmokePackagedReceiver`. This path does **not** start an internal Unreal
sender. It consumes an external nozzle source, copies acquired frames into an
Unreal render target through the native receiver component, samples that target
through the cooked `/Game/NozzleSmoke/NozzleSmokeReceiverMaterial` material, and
then validates both:

- material-output RGB/orientation/moving-marker samples; and
- direct receiver-target RGBA samples, including the alpha patch.

The cooked material asset is intentionally stored under `Content/NozzleSmoke/`
and `Config/DefaultGame.ini` explicitly cooks `/Game/NozzleSmoke`. Do not remove
that cook rule: the packaged receiver uses a string `LoadObject` path, so the
cooker will not discover the asset from a C++ hard reference.

Run the external `nozzle-viewer` sender first and keep it alive long enough for
the packaged receiver to observe multiple frames:

```sh
/path/to/nozzle-viewer \
  --smoke-sender \
  --source NozzleViewerSmoke320 \
  --width 320 \
  --height 240 \
  --frames 1200 \
  --interval-ms 16 \
  --evidence /tmp/nozzle-unreal-packaged-receiver-viewer-sender-320.json
```

Package the sample, then run the packaged receiver against that source:

```sh
'/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh' \
  BuildCookRun \
  -project='/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
  -noP4 -platform=Mac -clientconfig=Development -build -cook -stage -package

'/path/to/nozzle-unreal/Samples/NozzleSmoke/Binaries/Mac/NozzleSmoke.app/Contents/MacOS/NozzleSmoke' \
  -NozzleSmokePackagedReceiver \
  -NozzleSmokeWidth=320 \
  -NozzleSmokeHeight=240 \
  -NozzleSmokeSource=NozzleViewerSmoke320 \
  -NozzleSmokeStrictPass \
  -stdout -FullStdOutLogOutput
```

Expected receiver log markers:

```text
NOZZLE_RECEIVER_SMOKE_CONFIG packaged=1
NOZZLE_RECEIVER_SMOKE_START packaged=1
NOZZLE_RECEIVER_SMOKE_RESULT packaged=1 row_status=PASS_CANDIDATE
NOZZLE_RECEIVER_SMOKE_EXIT packaged=1 success=1
```

A packaged receiver/material PASS candidate must include the explicit Metal
`nozzle_frame_to_unreal_metal_texture` transfer path, native receiver target
details containing `receiver_target=FRHITexture::GetNativeResource`,
`MTLPixelFormat=81`, `storageMode=`, `usage=`, `iosurface_backed=`, and
`iosurface_id=`, plus the synchronization boundary text naming
`nozzle_frame_copy_to_native_texture` and the Metal backend copy wait. Timeout or
`row_status=MISSING` is always a failure for the packaged receiver/material
harness, even without `-NozzleSmokeStrictPass`.
