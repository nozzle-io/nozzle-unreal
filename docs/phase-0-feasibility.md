# Phase 0 feasibility boundary

Current scaffold status:

- Unreal plugin descriptor exists.
- Runtime and editor module skeletons exist.
- `NozzleCore.Build.cs` describes where staged nozzle headers/libs must live.
- Static shape/package CI exists.

What is not proven yet:

- Unreal Engine compilation.
- nozzle native library linking inside Unreal.
- safe render-thread access to `FRHITexture::GetNativeResource()`.
- `ID3D11Texture2D*` lifetime/synchronization.
- Unreal sender or receiver runtime behavior.
- packaged game behavior.

First real support target for future proof:

- Win64.
- D3D11 RHI only.
- BGRA8 before any RGBA8/swizzle/origin claim.
- Editor PIE and packaged Development builds as separate evidence.

Any issue comment, README, release note, or CI badge that describes this scaffold as working Unreal runtime support is false until the above evidence exists.
