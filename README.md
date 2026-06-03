# nozzle-unreal

Phase 0 scaffold for a future Unreal Engine plugin for [nozzle](https://github.com/nozzle-io/nozzle) GPU texture sharing.

This repository is intentionally conservative: it does **not** claim Unreal runtime support, Unreal build coverage, native nozzle linking, or a working texture path. CI now includes Unreal-independent CMake compile checks on the actual GitHub runner platforms: Windows for the D3D11 seam, macOS for the Metal seam, and Linux for the explicit unsupported guard. That still is not Unreal Engine, UHT, native linking, or runtime evidence.

## Current support status

| Area | Status |
| --- | --- |
| Plugin descriptor | Present |
| Runtime module skeleton | Present |
| Editor module skeleton | Present |
| nozzle native dependency staging | Placeholder only |
| Unreal-independent native bridge | CMake object compile checks against `deps/nozzle/include`; Windows builds the D3D11 seam, macOS builds the Metal seam, Linux builds the unsupported guard; no Unreal headers, no native link, no runtime execution |
| Unreal Engine compile | Not proven; CI remains static unless an engine is present |
| Runtime sender/receiver | Static Win64 D3D11 and macOS Metal source-level API skeleton plus native bridge seams; not engine-compiled |
| First intended RHI proof | Win64 + D3D11 |
| Next source-level RHI path | macOS + Metal |
| D3D12/Linux support | Not claimed |

A green static CI run means only that files are shaped correctly. It is not engine-build or runtime evidence.

## Layout

```text
Nozzle/
  Nozzle.uplugin
  Source/
    Nozzle/                    # runtime module skeleton
      Private/Native/          # Unreal-independent native bridge compile check
    NozzleEditor/              # editor module skeleton
    ThirdParty/NozzleCore/     # external module placeholder
  ThirdParty/nozzle/           # future staged headers/libs payload
  Resources/
deps/nozzle/                   # development submodule for nozzle-dev sync tooling
Samples/NozzleSmoke/           # future smoke-test project skeleton
docs/
scripts/
```

The development submodule is `deps/nozzle`, not root `nozzle`. A root `nozzle` directory collides with the `Nozzle/` plugin directory on case-insensitive macOS filesystems.

## First real target

The first implementation target is deliberately narrow:

- Unreal Engine 5.x, pinned to the exact installed version on the build/smoke machine.
- Win64.
- D3D11 RHI.
- BGRA8 first, before any RGBA8, origin, or channel-order claim.
- Editor PIE and packaged Development build evidence tracked separately.

The macOS source path is now allowed to advance in parallel as a Metal-only seam, but it is not a runtime support claim until Unreal Engine compilation, `FRHITexture::GetNativeResource()` behavior, IOSurface backing, synchronization, and live smoke are proven.

Do not broaden this list from optimism. Unreal's RHI abstraction does not make texture sharing portable by default.

## Runtime API skeleton boundary

The runtime module now exposes the first real API surface:

- `UNozzleSenderComponent` for publishing a `UTextureRenderTarget2D` through a native texture path.
- `UNozzleReceiverComponent` for copying an acquired nozzle frame into a `UTextureRenderTarget2D`.
- `UNozzleRuntimeBlueprintLibrary` for RHI/core diagnostics and sender discovery.
- `FNozzleRuntimeDiagnostics` for Blueprint-visible state, selected RHI, backend, dimensions, transfer-mode notes, and failure messages.

This is deliberately guarded. Runtime calls are blocked unless one of these source-level platform/RHI gates is true and `WITH_NOZZLE_CORE=1`: Win64 + selected Unreal RHI name contains `D3D11`, or macOS + selected Unreal RHI name contains `Metal`. D3D12 and Linux are not supported by this runtime path.

The code references Unreal RHI APIs (`GDynamicRHI`, `FTextureRenderTargetResource`, `FRHITexture::GetNativeResource`) but static CI does not compile or execute them because no Unreal Engine install is available in repository CI. Treat the skeleton as source-level implementation progress, not runtime evidence. The sender path now defers creation to the render thread and calls the shared native bridge's `nozzle_sender_create_with_native_device(...)` path after extracting Unreal's native D3D11 device/context or Metal device. Pending render commands no longer capture the component object directly; they use a thread-safe render state, flush on stop, publish/copy check cancellation immediately before executing GPU work, and expose last render-operation diagnostics plus a monotonically increasing render sequence for smoke tests. That still needs engine-backed pointer-lifetime, synchronization, IOSurface, and smoke proof before support can be claimed.


## Unreal-independent native bridge boundary

`Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.*` is a deliberately small C++ layer with no Unreal headers. Its implementation is header-inline so both the CMake compile-check and the Unreal module call the same native-device creation/publish/copy seam. It accepts D3D11 device/context pointers, Metal device pointers, and native texture pointers as opaque `void*` values, builds nozzle C descriptors, and compiles the guarded calls to:

- `nozzle_sender_create_with_native_device`
- `nozzle_sender_publish_native_texture_ex`
- `nozzle_frame_copy_to_native_texture`

The root `CMakeLists.txt` builds this as object-only compile targets so GitHub CI can validate the native API path and the disabled-core guard without Unreal Engine or staged nozzle binaries. Windows CI validates the D3D11 seam on a Windows runner; macOS CI validates the Metal seam on a macOS runner; Linux CI validates the unsupported guard. This is stronger than package-only CI, but it still does not prove Unreal RHI extraction, D3D11 synchronization, Metal IOSurface backing, native nozzle linking, UHT reflection, Editor PIE, or packaged runtime behavior.

The plugin-tree native bridge is now inside the plugin tree. That fixes the previous repo-root `Native/` package-boundary trap and makes the source layout compatible with a normal Unreal plugin-shaped package. BuildPlugin/UHT is still not proven: until `RunUAT BuildPlugin` runs against an installed Unreal Engine, this repository only has static package-shape and native bridge compile evidence.

The injected native device is treated as borrowed from Unreal, not owned by nozzle-unreal. The plugin must keep the sender lifetime inside Unreal graphics-device lifetime boundaries; this is another reason runtime support cannot be claimed before real engine lifecycle smoke exists.

## Native nozzle staging contract

`Nozzle/Source/ThirdParty/NozzleCore/NozzleCore.Build.cs` only enables `WITH_NOZZLE_CORE=1` when all expected staged files exist:

```text
Nozzle/ThirdParty/nozzle/include/nozzle/nozzle_c.h
Nozzle/ThirdParty/nozzle/lib/Win64/nozzle.lib
Nozzle/ThirdParty/nozzle/bin/Win64/nozzle.dll
Nozzle/ThirdParty/nozzle/lib/Mac/libnozzle.dylib
```

On Win64 the module also registers `nozzle.dll` as a delay-loaded runtime dependency and stages it through `RuntimeDependencies`. On Mac the dylib is both the link library and runtime dependency. Until those files are produced by a real build/package process, the module remains a placeholder and must not be used as linking evidence.

Native staging contract check:

```bash
python3 scripts/check_native_staging.py
python3 scripts/stage_native_nozzle.py --platform Mac
python3 scripts/stage_native_nozzle.py --platform Win64
python3 scripts/check_native_staging.py --require Mac --inspect-deps
python3 scripts/check_native_staging.py --require Win64 --inspect-deps
```

The default command accepts the current empty placeholder state but fails on partial staging. The `--require` mode is for a runner that is supposed to prove native linking; it fails if any required header, link library, runtime library, or dependency-inspection tool is missing.
In `--require` mode, native-link evidence is intentionally rejected unless `--inspect-deps` is also present. The checker validates that staged paths are real non-empty files, records size and SHA-256 evidence, checks that `nozzle_c.h` exposes the expected public C API surface, and parses platform dependency output instead of treating `otool`/`dumpbin` exit status as proof. On macOS, the dylib install name must use a package-loadable path such as `@rpath/`, `@loader_path/`, or `@executable_path/`; absolute build-machine paths and unstaged non-system dylib dependencies fail. On Win64, `dumpbin /DEPENDENTS` output is parsed and unexpected plugin-private DLL dependencies must be staged beside `nozzle.dll`.

Do not use the default command as native-link evidence. A real #139 evidence run needs the target-pinned strict checker (`--require <Mac|Win64> --inspect-deps`) plus Unreal engine compile/link output:

```bash
python3 scripts/check_native_staging.py --require Mac --inspect-deps
python3 scripts/run_build_plugin.py --runuat /path/to/Engine/Build/BatchFiles/RunUAT.sh --target-platform Mac --package build/BuildPlugin/Nozzle-Mac
```

or the equivalent Win64 target. If the BuildPlugin log does not prove `WITH_NOZZLE_CORE=1`, the native staging/link claim is still unproven.

## Validation commands

Static shape check, including runtime-source validation for the D3D11 guard, unsupported-RHI diagnostics, component classes, native bridge files, `WITH_NOZZLE_CORE` behavior, and absence of false D3D12/Linux support claims and false macOS runtime-support claims:

```bash
python3 scripts/check_package_shape.py
python3 scripts/check_native_staging.py
```

Unreal-independent native bridge compile check:

```bash
cmake -S . -B build/native-ci -DNOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=ON
cmake --build build/native-ci --target nozzle_unreal_native_bridge nozzle_unreal_native_bridge_compile_check
cmake -S . -B build/native-ci-disabled -DNOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=OFF
cmake --build build/native-ci-disabled --target nozzle_unreal_native_bridge nozzle_unreal_native_bridge_compile_check
```

These commands compile object files only. They intentionally do not link a nozzle library, load Unreal Engine, run UHT, or execute a D3D11 runtime smoke.

Source package shape check:

```bash
python3 scripts/package_source.py --output build/nozzle-unreal-scaffold.zip --root-name nozzle-unreal-scaffold
python3 scripts/check_package_shape.py --package build/nozzle-unreal-scaffold.zip
```

These commands do not invoke Unreal Engine. This static validation and native bridge compile check does not invoke Unreal Engine as part of CI. That omission is deliberate and must stay visible until real engine CI exists.

Engine-backed BuildPlugin validation, when an Unreal Engine install is available:

```bash
python3 scripts/run_build_plugin.py --runuat /path/to/Engine/Build/BatchFiles/RunUAT.sh --target-platform Win64 --package build/BuildPlugin/Nozzle-Win64
```

The script fails if `RunUAT` cannot be found. A missing engine is a blocker, not a reason to relabel static CI as BuildPlugin coverage. Normal engine-backed evidence must pass `--target-platform Win64` or `--target-platform Mac`; the script then passes `-TargetPlatforms=<value>` to `RunUAT`. After `RunUAT` returns, target-pinned evidence requires non-empty `Binaries/Win64/` or `Binaries/Mac/` even when the package also contains `Source/`. Binary-only packages are still validated against the selected target. Source-only package acceptance requires the explicit diagnostic-only `--allow-source-only-artifact` opt-out and is not #142 completion evidence. Running without a pinned target requires the explicit diagnostic-only `--allow-runuat-default-target` opt-out. The script asserts the package shape before printing the package tree: `Nozzle.uplugin` must exist, package-root `Native/` and development `deps/` are forbidden, `Saved/` and `DerivedDataCache/` are rejected, required source files are checked when `Source/` exists, and target-specific binary evidence is checked unless source-only diagnostic mode is explicitly selected. A real `RunUAT BuildPlugin` package may include package-root `Intermediate/`; that is artifact evidence, not something to commit back to the repository.

Existing package assertion, useful for reviewing archived BuildPlugin output:

```bash
python3 scripts/run_build_plugin.py --assert-package-only --package /path/to/Packaged/Nozzle --target-platform Win64 --expect-layout auto
```

For an archived source-only package, add `--allow-source-only-artifact` and report it only as diagnostic evidence. Do not use that mode as #142 acceptance evidence.

Manual GitHub Actions entry point:

```text
Actions -> Unreal BuildPlugin -> Run workflow
```

That workflow still requires a runner with Unreal Engine already installed and a valid `RunUAT` path. The `runner_labels_json` input is a JSON array, for example `["self-hosted","Windows","Unreal"]` or `["self-hosted","macOS","Unreal"]`. A hosted image is valid only if it is proven to contain the exact requested engine path. Third-party UE project-build Actions are wrappers around `RunUAT`; they do not remove the engine installation requirement.

For #139 native-link evidence, the workflow input `native_staging_mode` must be set to `require`. When staged binaries are not already committed in the checkout, `build_native_payload` must also be enabled so the workflow runs `scripts/stage_native_nozzle.py` before the strict staging check. The default `placeholder` mode is only for source/engine boundary checks and can still prove `WITH_NOZZLE_CORE=0`; it is not native nozzle link evidence. A valid #139 workflow run must show the native payload build step, the strict staging command, the `NozzleCore: WITH_NOZZLE_CORE=1` line emitted by `NozzleCore.Build.cs`, and the post-BuildPlugin packaged-payload check against `build/BuildPlugin/Nozzle-<target>/ThirdParty/nozzle`.

## Runtime evidence required later

See `docs/runtime-smoke-matrix.md`. Minimum proof before any runtime support claim:

- Unreal sender -> `nozzle-viewer`.
- `nozzle-viewer` or `nozzle-mixer` -> Unreal render target/material.
- Editor PIE and packaged Development separately.
- 320x240 and 641x479.
- D3D11, native format, transfer mode, no vertical flip, no R/B swap, and cleanup behavior recorded.

## Issue tracking

All roadmap and implementation issues stay in `nozzle-io/nozzle-dev`. Do not move tracking into this repository.
