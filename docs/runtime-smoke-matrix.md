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
| Unreal sender -> nozzle-viewer | Editor PIE | 320x240 | Metal RHI, IOSurface-backed texture proof, native format, no flip, no R/B swap | MISSING |
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
