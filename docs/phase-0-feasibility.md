# Phase 0 feasibility boundary

Current scaffold status:

- Unreal plugin descriptor exists.
- Runtime and editor module skeletons exist.
- `NozzleCore.Build.cs` describes where staged nozzle headers/libs must live.
- Runtime API skeleton exists for D3D11-only sender, receiver, and Blueprint diagnostics.
- Static shape/package CI exists and validates the D3D11 guard, unsupported-RHI diagnostics, component classes, and `WITH_NOZZLE_CORE` behavior.
- Unreal-independent CMake object compile checks now validate a small native D3D11/nozzle bridge seam against `deps/nozzle/include` without Unreal headers.

What is not proven yet:

- Unreal Engine compilation.
- nozzle native library linking inside Unreal.
- native nozzle library linking in the CMake bridge; the current CMake target is compile-only.
- safe render-thread access to `FRHITexture::GetNativeResource()`.
- `ID3D11Texture2D*` lifetime/synchronization.
- native D3D11 device/context injection from Unreal into nozzle; the native bridge can build `NozzleNativeDevice` from opaque D3D11 pointers and compile `nozzle_sender_create_with_native_device`, but Unreal still must prove device ownership and synchronization under an engine.
- Unreal sender or receiver runtime behavior under an installed engine.
- packaged game behavior.

First real support target for future proof:

- Win64.
- D3D11 RHI only.
- BGRA8 before any RGBA8/swizzle/origin claim.
- Editor PIE and packaged Development builds as separate evidence.

Any issue comment, README, release note, or CI badge that describes this scaffold as working Unreal runtime support is false until the above evidence exists. Source-level runtime APIs and a native compile-checked bridge seam are present, but Unreal compilation, native linking, and D3D11 execution are still blockers.
