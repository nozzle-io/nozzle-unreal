# Runtime smoke matrix required before support claims

Do not mark any row PASS without command output, logs, screenshots or captured frames, and exact SHAs.

| Direction | Host mode | Size | Required evidence | Status |
| --- | --- | --- | --- | --- |
| Unreal sender -> nozzle-viewer | Editor PIE | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Editor PIE | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 320x240 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| Unreal sender -> nozzle-viewer | Packaged Development | 641x479 | D3D11 RHI, BGRA/native format, no flip, no R/B swap | MISSING |
| nozzle-viewer/nozzle-mixer -> Unreal receiver/material | Editor PIE | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer/nozzle-mixer -> Unreal receiver/material | Editor PIE | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer/nozzle-mixer -> Unreal receiver/material | Packaged Development | 320x240 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |
| nozzle-viewer/nozzle-mixer -> Unreal receiver/material | Packaged Development | 641x479 | D3D11 RHI, render target update, no flip, no R/B swap | MISSING |

Every PASS record must include:

- Unreal version.
- nozzle-unreal commit SHA.
- nozzle core commit SHA.
- GPU and driver.
- Windows version.
- selected RHI.
- native texture format.
- transfer mode.
- EndPlay, level reload, and cleanup result.
