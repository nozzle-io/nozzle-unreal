# Phase 0 feasibility boundary

Current scaffold status:

- Unreal plugin descriptor exists.
- Runtime and editor module skeletons exist.
- `NozzleCore.Build.cs` describes where staged nozzle headers/libs must live.
- Runtime API skeleton exists for D3D11-only sender, receiver, and Blueprint diagnostics.
- Static shape/package CI exists and validates the D3D11 guard, unsupported-RHI diagnostics, component classes, and `WITH_NOZZLE_CORE` behavior.

What is not proven yet:

- Unreal Engine compilation.
- nozzle native library linking inside Unreal.
- safe render-thread access to `FRHITexture::GetNativeResource()`.
- `ID3D11Texture2D*` lifetime/synchronization.
- native D3D11 device/context injection from Unreal into nozzle; the current skeleton can queue native-texture calls but must still prove device ownership and synchronization under an engine.
- Unreal sender or receiver runtime behavior under an installed engine.
- packaged game behavior.

First real support target for future proof:

- Win64.
- D3D11 RHI only.
- BGRA8 before any RGBA8/swizzle/origin claim.
- Editor PIE and packaged Development builds as separate evidence.

Any issue comment, README, release note, or CI badge that describes this scaffold as working Unreal runtime support is false until the above evidence exists. Source-level runtime APIs are present, but Unreal compilation and D3D11 execution are still blockers.
