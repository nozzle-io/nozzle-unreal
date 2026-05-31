# nozzle-unreal

Phase 0 scaffold for a future Unreal Engine plugin for [nozzle](https://github.com/nozzle-io/nozzle) GPU texture sharing.

This repository is intentionally conservative: it does **not** claim Unreal runtime support, Unreal build coverage, native nozzle linking, or a working texture path. CI now includes an Unreal-independent CMake compile check for the native D3D11/nozzle bridge seam, but that still is not Unreal Engine, UHT, linking, or runtime evidence.

## Current support status

| Area | Status |
| --- | --- |
| Plugin descriptor | Present |
| Runtime module skeleton | Present |
| Editor module skeleton | Present |
| nozzle native dependency staging | Placeholder only |
| Unreal-independent native bridge | CMake object compile check against `deps/nozzle/include`; no Unreal headers, no native link, no runtime execution |
| Unreal Engine compile | Not proven; CI remains static unless an engine is present |
| Runtime sender/receiver | Static D3D11-only API skeleton plus native bridge seam; not engine-compiled |
| First intended RHI proof | Win64 + D3D11 |
| D3D12/macOS/Linux support | Not claimed |

A green static CI run means only that files are shaped correctly. It is not engine-build or runtime evidence.

## Layout

```text
Nozzle/
  Nozzle.uplugin
  Source/
    Nozzle/                    # runtime module skeleton
    NozzleEditor/              # editor module skeleton
    ThirdParty/NozzleCore/     # external module placeholder
  ThirdParty/nozzle/           # future staged headers/libs payload
  Resources/
deps/nozzle/                   # development submodule for nozzle-dev sync tooling
Samples/NozzleSmoke/           # future smoke-test project skeleton
Native/                         # Unreal-independent native bridge compile check
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

Do not broaden this list from optimism. Unreal's RHI abstraction does not make texture sharing portable by default.

## Runtime API skeleton boundary

The runtime module now exposes the first real API surface:

- `UNozzleSenderComponent` for publishing a `UTextureRenderTarget2D` through a D3D11 native texture path.
- `UNozzleReceiverComponent` for copying an acquired nozzle frame into a `UTextureRenderTarget2D`.
- `UNozzleRuntimeBlueprintLibrary` for RHI/core diagnostics and sender discovery.
- `FNozzleRuntimeDiagnostics` for Blueprint-visible state, selected RHI, backend, dimensions, transfer-mode notes, and failure messages.

This is deliberately guarded. Runtime calls are blocked unless all of these are true: Win64 target, selected Unreal RHI name contains `D3D11`, and `WITH_NOZZLE_CORE=1`. D3D12, macOS, and Linux are not supported by this runtime path.

The code references Unreal RHI APIs (`GDynamicRHI`, `FTextureRenderTargetResource`, `FRHITexture::GetNativeResource`) but static CI does not compile or execute them because no Unreal Engine install is available in repository CI. Treat the skeleton as source-level implementation progress, not runtime evidence. The current sender creation path is intentionally conservative and still needs an engine-backed D3D11 device/context proof before support can be claimed.


## Unreal-independent native bridge boundary

`Native/nozzle_unreal_native_bridge.*` is a deliberately small C++ layer with no Unreal headers. It accepts D3D11 device/context/texture pointers as opaque `void*` values, builds nozzle C descriptors, and compiles the guarded calls to:

- `nozzle_sender_create_with_native_device`
- `nozzle_sender_publish_native_texture_ex`
- `nozzle_frame_copy_to_native_texture`

The root `CMakeLists.txt` builds this as object-only compile targets so GitHub CI can validate the native API path and the disabled-core guard without Unreal Engine or a staged `nozzle.dll`. This is stronger than package-only CI, but it still does not prove Unreal RHI extraction, D3D11 synchronization, native nozzle linking, UHT reflection, Editor PIE, or packaged runtime behavior.

## Native nozzle staging contract

`Nozzle/Source/ThirdParty/NozzleCore/NozzleCore.Build.cs` only enables `WITH_NOZZLE_CORE=1` when all expected staged files exist:

```text
Nozzle/ThirdParty/nozzle/include/nozzle/nozzle_c.h
Nozzle/ThirdParty/nozzle/lib/Win64/nozzle.lib
Nozzle/ThirdParty/nozzle/bin/Win64/nozzle.dll
```

Until those files are produced by a real build/package process, the module remains a placeholder and must not be used as linking evidence.

## Validation commands

Static shape check, including runtime-source validation for the D3D11 guard, unsupported-RHI diagnostics, component classes, native bridge files, `WITH_NOZZLE_CORE` behavior, and absence of false D3D12/macOS/Linux support claims:

```bash
python3 scripts/check_package_shape.py
```

Unreal-independent native bridge compile check:

```bash
cmake -S . -B build/native-ci -DNOZZLE_UNREAL_NATIVE_FORCE_WIN64_D3D11=ON -DNOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=ON
cmake --build build/native-ci --target nozzle_unreal_native_bridge nozzle_unreal_native_bridge_compile_check
cmake -S . -B build/native-ci-disabled -DNOZZLE_UNREAL_NATIVE_FORCE_WIN64_D3D11=ON -DNOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=OFF
cmake --build build/native-ci-disabled --target nozzle_unreal_native_bridge nozzle_unreal_native_bridge_compile_check
```

These commands compile object files only. They intentionally do not link a nozzle library, load Unreal Engine, run UHT, or execute a D3D11 runtime smoke.

Source package shape check:

```bash
python3 scripts/package_source.py --output build/nozzle-unreal-scaffold.zip --root-name nozzle-unreal-scaffold
python3 scripts/check_package_shape.py --package build/nozzle-unreal-scaffold.zip
```

These commands do not invoke Unreal Engine. This static validation and native bridge compile check does not invoke Unreal Engine as part of CI. That omission is deliberate and must stay visible until real engine CI exists.

## Runtime evidence required later

See `docs/runtime-smoke-matrix.md`. Minimum proof before any runtime support claim:

- Unreal sender -> `nozzle-viewer`.
- `nozzle-viewer` or `nozzle-mixer` -> Unreal render target/material.
- Editor PIE and packaged Development separately.
- 320x240 and 641x479.
- D3D11, native format, transfer mode, no vertical flip, no R/B swap, and cleanup behavior recorded.

## Issue tracking

All roadmap and implementation issues stay in `nozzle-io/nozzle-dev`. Do not move tracking into this repository.
