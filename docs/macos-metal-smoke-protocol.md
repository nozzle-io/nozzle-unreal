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
3. `nozzle-viewer` -> Unreal receiver/material.
4. `nozzle-mixer` -> Unreal receiver/material.

The receiver/material rows are intentionally split. A `nozzle-viewer` source
and a `nozzle-mixer` output source exercise different producer behavior; do
not collapse them into a combined "viewer or mixer" row when recording PASS
evidence.

## Evidence fields per row

- Command line or editor launch path.
- RHI confirmation showing Metal.
- Unreal render target size and native Metal texture format.
- Native Metal texture details:
  - `MTLPixelFormat`.
  - width and height.
  - `storageMode`.
  - `usage` flags where observable.
  - whether the texture pointer came directly from `FRHITexture::GetNativeResource()` or from an intermediate texture.
- Texture sharing/backing outcome for the source or target texture where required. Every attempted row must state exactly one of:
  - PASS: the Unreal source/target `id<MTLTexture>` itself is IOSurface-backed, with non-null IOSurface proof and an IOSurface ID;
  - PASS: the plugin copies through a named IOSurface-backed intermediate texture, with the transfer mode documented;
  - PASS: receiver-side rows copy an acquired nozzle frame through an explicitly named Metal frame-to-target path into the Unreal target texture, while recording whether that target itself is IOSurface-backed;
  - MISSING/FAIL: the tested texture is not cross-process shareable and no explicit copy path was proven.
- Synchronization boundary. A PASS row must name the actual ordering mechanism:
  - command buffer completion handler or wait;
  - fence or shared event;
  - explicit flush/blit completion;
  - or a documented reason why the tested path is already ordered.
- Sender/receiver names.
- Last render diagnostics sequence before and after the row.
- Multi-frame sequence evidence proving the receiver is not showing a stale frame.
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

## Test pattern requirement

Every PASS row must use a non-symmetric source image or generated test pattern.
The pattern must include:

- distinct visual labels or colors in all four corners;
- separated red and blue regions;
- a deliberate alpha patch over a visible background;
- a visible size/orientation cue that differentiates `320x240` from `641x479`;
- a frame counter or changing marker for stale-frame detection.

A symmetric checkerboard, solid color, or single screenshot without a changing
frame marker is not enough to prove no vertical flip, no R/B swap, alpha
behavior, or synchronization correctness.
