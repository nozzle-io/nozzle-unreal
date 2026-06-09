# NozzleSmoke sample

Minimal Unreal sample project for nozzle runtime diagnostics. It contains an
Editor PIE automation test for the smallest macOS Metal row:

```text
Nozzle.Smoke.MacMetal.UnrealSenderToViewer.EditorPIE.320x240
```

The test creates a transient 320x240 `UTextureRenderTarget2D`, draws a
non-symmetric quadrant pattern with a moving frame marker, publishes it through
`UNozzleSenderComponent`, and logs `NOZZLE_SMOKE_*` diagnostics including native
Metal texture details, IOSurface state, synchronization text, dimensions, and
multi-frame render sequence.

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

Add `-NozzleSmokeStrictPass` when the run must fail unless the row satisfies the
strict cross-process Metal acceptance gate. Without that flag, the automation
logs `row_status=MISSING` instead of failing when Unreal's texture is not
IOSurface-backed.

## Evidence boundary

A successful automation process is **not runtime evidence** by itself. Treat the
row as PASS only when the issue evidence includes:

- exact Unreal Engine, `nozzle-unreal`, and nozzle core SHAs;
- Metal RHI confirmation;
- `NativeTextureDetails`, `bIOSurfaceBacked`, and `IOSurfaceID`;
- concrete synchronization boundary/outcome;
- multi-frame marker / stale-frame evidence;
- captured `nozzle-viewer` output proving dimensions, orientation, channel order,
  and alpha behavior.

If `row_status=MISSING` or the viewer reports failed quadrant semantics, keep the
runtime matrix row MISSING/FAIL and split the focused bug instead of claiming
Metal runtime support.
