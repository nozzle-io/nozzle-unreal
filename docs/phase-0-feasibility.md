# Phase 0 feasibility boundary

Current scaffold status:

- Unreal plugin descriptor exists for Win64 and Mac source-level targets.
- Runtime and editor module skeletons exist.
- `NozzleCore.Build.cs` describes where staged nozzle headers/libs must live for Win64 and Mac.
- Runtime API skeleton exists for sender, receiver, and Blueprint diagnostics.
- The Unreal source gate now distinguishes Win64 D3D11, macOS Metal, and unsupported platforms/RHIs.
- Static shape/package CI exists and validates the platform/RHI guard, unsupported-RHI diagnostics, component classes, and `WITH_NOZZLE_CORE` behavior.
- Unreal-independent CMake object compile checks validate native nozzle bridge seams against `deps/nozzle/include` without Unreal headers:
  - Windows runner: D3D11 native-device/native-texture seam.
  - macOS runner: Metal native-device/native-texture seam.
  - Linux runner: explicit unsupported guard.

What is not proven yet:

- Unreal Engine compilation.
- UHT/reflection generation.
- nozzle native library linking inside Unreal.
- native nozzle library linking in the CMake bridge; the current CMake target is compile-only.
- staged native nozzle payloads: `scripts/check_native_staging.py` rejects partial staging and can require platform payloads plus dependency inspection, but no staged nozzle binaries are present in this scaffold.
- safe render-thread access to `FRHITexture::GetNativeResource()` under either D3D11 or Metal. The source now extracts D3D11 device/context from the native texture and Metal device from `id<MTLTexture>`, routes render commands through a thread-safe state object instead of capturing the component, exposes last render-operation diagnostics plus a monotonically increasing render sequence for smoke tests, and checks cancellation before publish/copy, but none of that is engine-executed evidence.
- `ID3D11Texture2D*` lifetime/synchronization.
- Metal `id<MTLTexture>` lifetime/synchronization.
- explicit borrowed-device lifetime proof: nozzle core stores injected native devices as borrowed pointers, so Unreal must prove the engine device outlives every sender created from it.
- BuildPlugin/UHT package proof: the native bridge now lives under `Nozzle/Source/Nozzle/Private/Native/`, so the previous repo-root `Native/` package-boundary trap is removed. This is still not engine-backed evidence until `RunUAT BuildPlugin` compiles the runtime/editor modules under an installed Unreal Engine.
- Whether Unreal's macOS render targets are IOSurface-backed in the required configuration.
- native D3D11 device/context injection from Unreal into nozzle; the native bridge can build `NozzleNativeDevice` from opaque D3D11 pointers and compile `nozzle_sender_create_with_native_device`, but Unreal still must prove device ownership and synchronization under an engine.
- native Metal device injection from Unreal into nozzle under a real engine. The source-level `.mm` helper can extract `[Texture device]` from an `id<MTLTexture>` and feed the shared bridge, but Unreal still must prove pointer ownership, IOSurface backing, and synchronization under an engine.
- Unreal sender or receiver runtime behavior under an installed engine.
- packaged game behavior.

Engine-backed validation command when an engine exists:

```bash
python3 scripts/run_build_plugin.py --runuat /path/to/Engine/Build/BatchFiles/RunUAT.sh --target-platform Win64 --package build/BuildPlugin/Nozzle-Win64
```

If `RunUAT` is missing, that is a validation blocker. It must not be converted into a green static CI claim. If `RunUAT` succeeds, the package directory is still rejected unless `Nozzle.uplugin` exists, package-root `Native/` is absent, development `deps/` is absent, generated scratch directories are absent, and the package matches the expected source or binary layout. Evidence runs must pass `--target-platform Win64` or `--target-platform Mac`; binary-only assertions then require the matching `Binaries/<target>/` directory instead of accepting a mislabeled artifact. A no-target RunUAT call requires explicit `--allow-runuat-default-target` and is diagnostic-only, not acceptance evidence.

First real support target for future proof:

- Win64.
- D3D11 RHI only.
- BGRA8 before any RGBA8/swizzle/origin claim.
- Editor PIE and packaged Development builds as separate evidence.

Parallel source-level path that can advance before runtime support is claimed:

- macOS.
- Metal RHI only.
- BGRA8 first.
- Explicit IOSurface backing proof before any live texture-sharing claim.
- Editor PIE and packaged Development builds as separate evidence.

Any issue comment, README, release note, or CI badge that describes this scaffold as working Unreal runtime support is false until the above evidence exists. Source-level runtime APIs and native compile-checked bridge seams are present for Win64 D3D11 and macOS Metal, and the Unreal sender path now uses the shared native-device bridge instead of a default-device sender. Unreal compilation, native linking, and runtime execution are still blockers.
