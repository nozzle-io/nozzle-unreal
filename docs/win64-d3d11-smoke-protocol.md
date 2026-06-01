# Win64 D3D11 runtime smoke protocol

This protocol is for #137. It is not satisfied by static CI, CMake object
compile, or log-only success. Every PASS row must attach real frame evidence.

## Preconditions

- `RunUAT BuildPlugin` has passed for the exact `nozzle-unreal` SHA.
- Native nozzle payload staging/link validation has passed with `WITH_NOZZLE_CORE=1`.
- Windows runner or manual machine records:
  - Windows version.
  - GPU and driver.
  - Unreal Engine version.
  - `nozzle-unreal` SHA.
  - nozzle core SHA used for staged binaries.
- Unreal project is launched with D3D11, not D3D12.

## Required rows

For each host mode (`Editor PIE`, `Packaged Development`) and each size
(`320x240`, `641x479`), record all of:

1. Unreal sender -> `nozzle-viewer`.
2. Unreal sender -> `nozzle-mixer`.
3. `nozzle-viewer` or `nozzle-mixer` -> Unreal receiver/material.

## Evidence fields per row

- Command line or editor launch path.
- RHI confirmation showing D3D11.
- Unreal render target size and native DXGI format, where observable.
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

Do not mark a row PASS when only logs look clean. If the frame is black,
stale, flipped, channel-swapped, wrong-sized, or only visible in one host mode,
leave the row MISSING/FAIL and split a focused bug.
