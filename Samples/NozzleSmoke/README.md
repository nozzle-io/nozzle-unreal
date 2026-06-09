# NozzleSmoke sample

Minimal Unreal sample project for nozzle runtime diagnostics. It contains an
Editor PIE automation test for the smallest macOS Metal row:

```text
Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240
```

The test creates a transient 320x240 `UTextureRenderTarget2D`, draws a
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

4. Run the Editor PIE diagnostic:

   ```sh
   '/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor' \
     '/path/to/nozzle-unreal/Samples/NozzleSmoke/NozzleSmoke.uproject' \
     -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput \
     -ExecCmds='Automation RunTests Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240' \
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
   `nozzle-viewer` and save the JSON artifact:

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
     --evidence /tmp/nozzle-unreal-viewer-smoke.json
   ```

The viewer evidence must report at least five distinct frame indices in
`frame.observed_indices`, per-frame moving marker samples in
`frame.observed_marker_samples`, `checks.moving_marker=PASS`, and
`checks.distinct_frames=PASS`. Marker evidence is valid only when at least five
distinct frames have PASS samples with changing `x` positions and each sample
records `expected_rgba`, `actual_rgba`, and `passed`. The alpha patch sample at
`x=160,y=105` must appear as RGBA `[255,0,255,64]` via
`samples[].expected_rgba` / `samples[].actual_rgba`. A single correct-looking
frame is not freshness evidence.

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

If `row_status=MISSING` or the viewer reports failed quadrant semantics, keep the
runtime matrix row MISSING/FAIL and split the focused bug instead of claiming
Metal runtime support.
