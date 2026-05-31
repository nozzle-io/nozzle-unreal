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
- safe render-thread access to `FRHITexture::GetNativeResource()` under either D3D11 or Metal.
- `ID3D11Texture2D*` lifetime/synchronization.
- Metal `id<MTLTexture>` lifetime/synchronization.
- Whether Unreal's macOS render targets are IOSurface-backed in the required configuration.
- native D3D11 device/context injection from Unreal into nozzle; the native bridge can build `NozzleNativeDevice` from opaque D3D11 pointers and compile `nozzle_sender_create_with_native_device`, but Unreal still must prove device ownership and synchronization under an engine.
- native Metal device injection from Unreal into nozzle; the native bridge can build `NozzleNativeDevice` from an opaque Metal device pointer and compile `nozzle_sender_create_with_native_device`, but Unreal still must prove pointer ownership and synchronization under an engine.
- Unreal sender or receiver runtime behavior under an installed engine.
- packaged game behavior.

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

Any issue comment, README, release note, or CI badge that describes this scaffold as working Unreal runtime support is false until the above evidence exists. Source-level runtime APIs and native compile-checked bridge seams are present for Win64 D3D11 and macOS Metal, but Unreal compilation, native linking, and runtime execution are still blockers.
