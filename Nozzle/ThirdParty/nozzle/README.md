# Staged nozzle payload placeholder

This directory is intentionally empty in the Phase 0 scaffold.

Future release packages may stage nozzle headers and binaries here, for example:

```text
include/nozzle/nozzle_c.h
lib/Win64/nozzle.lib
bin/Win64/nozzle.dll
lib/Mac/libnozzle.dylib
```

The placeholder `NozzleCore.Build.cs` only defines `WITH_NOZZLE_CORE=1` when the required header, link library, and runtime library for the target platform are actually present. It must not pretend that native linking works without staged headers and binaries.

Use `scripts/check_native_staging.py` from the repository root to reject partial staging before trying Unreal BuildPlugin validation. The default checker only proves that the placeholder is empty or that staging is not partial; it is not native-link evidence.

Strict target evidence must use `--require <Mac|Win64> --inspect-deps`. That mode rejects missing files, directories, zero-byte files, headers that do not expose the expected nozzle public C API surface, macOS dylibs with non-package install names or unstaged non-system dependencies, and Win64 DLLs with unstaged plugin-private dependencies.

Mac BuildPlugin evidence must also run `scripts/check_buildplugin_module_dependencies.py` against the packaged plugin. Staging `libnozzle.dylib` is not enough if the built Unreal module records an absolute build-machine dependency path or lacks an `LC_RPATH` that can resolve the packaged dylib.
