# Staged nozzle payload placeholder

This directory is intentionally empty in the Phase 0 scaffold.

Future release packages may stage nozzle headers and binaries here, for example:

```text
include/nozzle/nozzle_c.h
lib/Win64/nozzle.lib
bin/Win64/nozzle.dll
```

The placeholder `NozzleCore.Build.cs` only defines `WITH_NOZZLE_CORE=1` when those files are actually present. It must not pretend that native linking works without staged headers and binaries.
