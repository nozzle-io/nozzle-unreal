# macOS Metal smoke protocol, not runtime support

This protocol is for #140. It is separate from Win64 D3D11 because Metal
requires different proof: `id<MTLTexture>` lifetime, IOSurface backing,
synchronization, channel order, and origin behavior.

## Preconditions

- `RunUAT BuildPlugin` has passed for the exact `nozzle-unreal` SHA.
- Native nozzle payload staging/link validation has passed with `WITH_NOZZLE_CORE=1`.
- The tested Unreal version compiles the source-level Metal path.
- Machine records:
  - macOS version.
  - Apple Silicon or Intel GPU details.
  - Unreal Engine version.
  - `nozzle-unreal` SHA.
  - nozzle core SHA used for staged binaries.
- Unreal project is launched with Metal RHI.

## Required rows

For each host mode (`Editor PIE`, `Packaged Development`) and each size
(`320x240`, `641x479`), record all of:

1. Unreal sender -> `nozzle-viewer`.
2. Unreal sender -> `nozzle-mixer`.
3. `nozzle-viewer` or `nozzle-mixer` -> Unreal receiver/material.

## Evidence fields per row

- Command line or editor launch path.
- RHI confirmation showing Metal.
- Unreal render target size and native Metal texture format.
- IOSurface backing proof for the source or target texture where required.
- Synchronization behavior and where GPU work is ordered.
- Sender/receiver names.
- Last render diagnostics sequence before and after the row.
- Screenshot or captured frame for the receiver side.
- Dimensions observed by the receiver.
- Vertical orientation: no flip / flip with proof.
- Channel order: no R/B swap / swap with proof.
- Alpha behavior.
- EndPlay cleanup result.
- Level reload cleanup result.
- Application quit cleanup result.

## Failure handling

Do not infer support from CMake Metal compile checks. If IOSurface backing,
texture lifetime, synchronization, channel order, or origin cannot be proven,
leave the row MISSING/FAIL and split a focused bug.
