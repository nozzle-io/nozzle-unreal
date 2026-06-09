# Runtime smoke matrix required before support claims

Do not mark any row PASS without command output, logs, screenshots or captured frames, and exact SHAs. The CMake native bridge compile check is useful preflight evidence, but it does not satisfy any Unreal Engine/UHT/runtime row below.

## Win64 D3D11 first support target

| Direction | Host mode | Size | Required evidence | Status |
| --- | --- | --- | --- | --- |
| Unreal sender -> nozzle-viewer | Editor PIE | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Editor PIE | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-mixer | Editor PIE | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Editor PIE | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Packaged Development | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Packaged Development | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| nozzle-viewer -> Unreal receiver/material | Editor PIE | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Editor PIE | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Packaged Development | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Packaged Development | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-mixer -> Unreal receiver/material | Editor PIE | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Editor PIE | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Packaged Development | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Packaged Development | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap, mixer output source selected | MISSING |

## macOS Metal source-level path, not support yet

| Direction | Host mode | Size | Required evidence | Status |
| --- | --- | --- | --- | --- |
| Unreal sender -> nozzle-viewer | Editor PIE | 320x240 | #158 PASS evidence: UE 5.7.4, macOS 15.4.1, Apple M4/Metal, `unreal_metal_blit_to_iosurface`, `MTLPixelFormat=80`, IOSurface-backed intermediate with non-zero IOSurface ID, `usage=0x3`, proof-first `RHICmdList.SubmitAndBlockUntilGPUIdle` + Metal blit `waitUntilCompleted`, viewer JSON proving 320x240, orientation/channel order, alpha patch, moving marker, and distinct frames; UE log shows `UWorld::CleanupWorld ... bSessionEnded=true, bCleanupResources=true` and automation `Result={Success}`. #159 revalidated the same row after replacing the global GPU-idle source sync with an `FRHIGPUFence` path: render thread writes the fence after Unreal source texture work, dispatches to the RHI thread, waits only that fence before the cross-queue IOSurface copy, then keeps the Metal intermediate blit `waitUntilCompleted` and nozzle ring publish completion wait; runtime log contained no `SubmitAndBlockUntilGPUIdle` or `RHIBlockUntilGPUIdle`; viewer JSON again proved 320x240, alpha patch, moving marker, and five distinct frames. SHAs: #158 nozzle-unreal `01d91aec9ade29068c5cf645c05177bed86ff158`, #159 nozzle-unreal `6ac4348b8889da9de5fe1b14bbaea315435bda1a`, nozzle-viewer `fb5426b4dbaabb71e53120a74800db14806f1d08`, core `65316f0b226db1ccd39380b14694157c41d08cc1`. Evidence: #158 final report https://github.com/nozzle-io/nozzle-dev/issues/158#issuecomment-4659413101, #140 follow-up https://github.com/nozzle-io/nozzle-dev/issues/140#issuecomment-4659472771, and #159 implementation report https://github.com/nozzle-io/nozzle-dev/issues/159#issuecomment-4660441337. | PASS (#158, #159) |
| Unreal sender -> nozzle-viewer | Editor PIE | 641x479 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 320x240 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 641x479 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-mixer | Editor PIE | 320x240 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Editor PIE | 641x479 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Packaged Development | 320x240 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| Unreal sender -> nozzle-mixer | Packaged Development | 641x479 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap, mixer preview/forwarding observed | MISSING |
| nozzle-viewer -> Unreal receiver/material | Editor PIE | 320x240 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Editor PIE | 641x479 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Packaged Development | 320x240 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer -> Unreal receiver/material | Packaged Development | 641x479 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap | MISSING |
| nozzle-mixer -> Unreal receiver/material | Editor PIE | 320x240 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Editor PIE | 641x479 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Packaged Development | 320x240 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap, mixer output source selected | MISSING |
| nozzle-mixer -> Unreal receiver/material | Packaged Development | 641x479 | Metal RHI, IOSurface-backed target proof, render target update, no flip, no R/B swap, mixer output source selected | MISSING |

Note: the PASS above is limited to the single macOS Metal Editor PIE 320x240 Unreal-sender-to-nozzle-viewer row proven in #158. It does not prove packaged builds, 641x479, nozzle-mixer forwarding, Unreal receiver/material rows, Win64 D3D11, or broad macOS Metal support; those rows remain MISSING until separately proven.

Every PASS record must include:

- Unreal version.
- nozzle-unreal commit SHA.
- nozzle core commit SHA.
- GPU and driver.
- OS version.
- selected RHI.
- native texture format.
- native texture backing proof: D3D11 resource details on Windows, IOSurface presence on macOS.
- transfer mode.
- non-symmetric source pattern with distinct corners, separated red/blue regions, an alpha patch, and visible size/orientation labels.
- synchronization boundary and multi-frame sequence/stale-frame check.
- EndPlay, level reload, and cleanup result.
