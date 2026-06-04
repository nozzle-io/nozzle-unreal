#!/usr/bin/env python3
"""Regression fixtures for BuildPlugin module dependency validation."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

from check_buildplugin_module_dependencies import (  # noqa: E402
    MAC_NOZZLE_DYLIB,
    MAC_NOZZLE_MODULE,
    MacosModuleEvidence,
    parse_macos_otool_rpaths,
    validate_macos_module_evidence,
)
from check_native_staging import MacosDependencySlice, parse_macos_otool_output  # noqa: E402


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def write_file(path: Path, text: str = "fixture\n") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def make_package(root: Path) -> Path:
    package_root = root / "Nozzle-Mac"
    write_file(package_root / "Binaries" / "Mac" / MAC_NOZZLE_MODULE, "Nozzle module fixture\n")
    write_file(package_root / "Binaries" / "Mac" / "UnrealEditor-NozzleEditor.dylib", "editor module fixture\n")
    write_file(package_root / MAC_NOZZLE_DYLIB, "libnozzle fixture\n")
    return package_root


def make_evidence(package_root: Path, module_name: str, dependencies: list[str], rpaths: list[str]) -> MacosModuleEvidence:
    return MacosModuleEvidence(
        module=package_root / "Binaries" / "Mac" / module_name,
        dependency_slices=[
            MacosDependencySlice(
                architecture="arm64",
                dependencies=dependencies,
            )
        ],
        rpaths=rpaths,
    )


def require_success(errors: list[str], label: str) -> None:
    if errors:
        fail(f"{label} should pass, got errors: {'; '.join(errors)}")
    print(f"PASS: {label}")


def require_failure(errors: list[str], expected: str, label: str) -> None:
    joined = "; ".join(errors)
    if not errors:
        fail(f"{label} should fail, got success")
    if expected not in joined:
        fail(f"{label} output is missing {expected!r}; errors: {joined}")
    print(f"PASS: {label}")


def assert_parser_fixtures() -> None:
    otool_l = """\
/pkg/Binaries/Mac/UnrealEditor-Nozzle.dylib (architecture x86_64):
    @rpath/UnrealEditor-Nozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
/pkg/Binaries/Mac/UnrealEditor-Nozzle.dylib (architecture arm64):
    @rpath/UnrealEditor-Nozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
"""
    slices = parse_macos_otool_output(otool_l)
    if [dependency_slice.architecture for dependency_slice in slices] != ["x86_64", "arm64"]:
        fail(f"fat otool -L parser did not preserve architectures: {slices}")
    if any("@rpath/libnozzle.dylib" not in dependency_slice.dependencies for dependency_slice in slices):
        fail("fat otool -L parser dropped @rpath/libnozzle.dylib")

    otool_load_commands = """\
Load command 18
          cmd LC_RPATH
      cmdsize 64
         path @loader_path/../../ThirdParty/nozzle/lib/Mac (offset 12)
Load command 19
          cmd LC_LOAD_DYLIB
"""
    rpaths = parse_macos_otool_rpaths(otool_load_commands)
    if rpaths != ["@loader_path/../../ThirdParty/nozzle/lib/Mac"]:
        fail(f"LC_RPATH parser returned {rpaths!r}")
    print("PASS: parser fixtures")


def main() -> None:
    assert_parser_fixtures()

    with tempfile.TemporaryDirectory(prefix="nozzle-unreal-module-deps-") as temp_root:
        package_root = make_package(Path(temp_root))

        valid_nozzle_module = make_evidence(
            package_root,
            MAC_NOZZLE_MODULE,
            [
                "@rpath/UnrealEditor-Nozzle.dylib",
                "@rpath/libnozzle.dylib",
                "/usr/lib/libc++.1.dylib",
            ],
            ["@loader_path/../../ThirdParty/nozzle/lib/Mac"],
        )
        require_success(validate_macos_module_evidence(package_root, valid_nozzle_module), "Nozzle module resolves @rpath libnozzle")

        absolute_build_path = make_evidence(
            package_root,
            MAC_NOZZLE_MODULE,
            [
                "@rpath/UnrealEditor-Nozzle.dylib",
                "/Users/build/nozzle/libnozzle.dylib",
            ],
            ["@loader_path/../../ThirdParty/nozzle/lib/Mac"],
        )
        require_failure(
            validate_macos_module_evidence(package_root, absolute_build_path),
            "build-machine path",
            "Nozzle module rejects absolute build-machine libnozzle",
        )

        missing_rpath = make_evidence(
            package_root,
            MAC_NOZZLE_MODULE,
            [
                "@rpath/UnrealEditor-Nozzle.dylib",
                "@rpath/libnozzle.dylib",
            ],
            [],
        )
        require_failure(
            validate_macos_module_evidence(package_root, missing_rpath),
            "no resolvable loader-path candidate",
            "Nozzle module rejects @rpath libnozzle without LC_RPATH",
        )

        wrong_rpath = make_evidence(
            package_root,
            MAC_NOZZLE_MODULE,
            [
                "@rpath/UnrealEditor-Nozzle.dylib",
                "@rpath/libnozzle.dylib",
            ],
            ["@loader_path/../ThirdParty/nozzle/lib/Mac"],
        )
        require_failure(
            validate_macos_module_evidence(package_root, wrong_rpath),
            "does not resolve to packaged",
            "Nozzle module rejects LC_RPATH that misses packaged libnozzle",
        )

        no_libnozzle = make_evidence(
            package_root,
            MAC_NOZZLE_MODULE,
            [
                "@rpath/UnrealEditor-Nozzle.dylib",
                "/usr/lib/libc++.1.dylib",
            ],
            ["@loader_path/../../ThirdParty/nozzle/lib/Mac"],
        )
        require_failure(
            validate_macos_module_evidence(package_root, no_libnozzle),
            "must link packaged libnozzle.dylib",
            "Nozzle module requires libnozzle dependency",
        )

        editor_without_libnozzle = make_evidence(
            package_root,
            "UnrealEditor-NozzleEditor.dylib",
            [
                "@rpath/UnrealEditor-NozzleEditor.dylib",
                "/usr/lib/libc++.1.dylib",
            ],
            [],
        )
        require_success(validate_macos_module_evidence(package_root, editor_without_libnozzle), "editor module may omit libnozzle")

    print("BuildPlugin module dependency regressions passed: 7 cases")


if __name__ == "__main__":
    main()
