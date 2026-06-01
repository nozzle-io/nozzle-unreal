#!/usr/bin/env python3
"""Run Unreal BuildPlugin when an Unreal Engine install is available.

This script is intentionally separate from the static CI workflow. It provides
the engine-backed validation command for the package boundary, but it must fail
when RunUAT is missing instead of pretending that static checks are BuildPlugin
coverage.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN_DESCRIPTOR = ROOT / "Nozzle" / "Nozzle.uplugin"

REPOSITORY_GENERATED_PARTS = {
    "Binaries",
    "DerivedDataCache",
    "Intermediate",
    "Saved",
}

PACKAGE_FORBIDDEN_PARTS = {
    "DerivedDataCache",
    "Intermediate",
    "Saved",
}

SOURCE_LAYOUT_REQUIRED_FILES = [
    "Nozzle.uplugin",
    "Source/Nozzle/Nozzle.Build.cs",
    "Source/Nozzle/Public/NozzleSenderComponent.h",
    "Source/Nozzle/Public/NozzleReceiverComponent.h",
    "Source/Nozzle/Private/NozzleNativeBridge.cpp",
    "Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.h",
    "Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.cpp",
    "Source/NozzleEditor/NozzleEditor.Build.cs",
    "Source/ThirdParty/NozzleCore/NozzleCore.Build.cs",
]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def resolve_runuat(args: argparse.Namespace) -> Path:
    candidates: list[Path] = []
    if args.runuat is not None:
        candidates.append(args.runuat)
    if os.environ.get("UE_RUNUAT"):
        candidates.append(Path(os.environ["UE_RUNUAT"]))
    if args.engine_root is not None:
        candidates.extend([
            args.engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh",
            args.engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.command",
            args.engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat",
            args.engine_root / "Build" / "BatchFiles" / "RunUAT.sh",
            args.engine_root / "Build" / "BatchFiles" / "RunUAT.command",
            args.engine_root / "Build" / "BatchFiles" / "RunUAT.bat",
        ])
    if os.environ.get("UE_ROOT"):
        engine_root = Path(os.environ["UE_ROOT"])
        candidates.extend([
            engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh",
            engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.command",
            engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat",
            engine_root / "Build" / "BatchFiles" / "RunUAT.sh",
            engine_root / "Build" / "BatchFiles" / "RunUAT.command",
            engine_root / "Build" / "BatchFiles" / "RunUAT.bat",
        ])

    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file():
            return resolved

    fail("RunUAT was not found. Pass --runuat, --engine-root, UE_RUNUAT, or UE_ROOT; static CI is not BuildPlugin evidence.")


def read_engine_version(runuat: Path) -> str:
    parts = runuat.parts
    engine_root: Path | None = None
    if "Engine" in parts:
        engine_index = parts.index("Engine")
        engine_root = Path(*parts[:engine_index + 1])
    elif len(parts) >= 3:
        engine_root = runuat.parents[2]
    if engine_root is None:
        return "unknown"

    version_file = engine_root / "Build" / "Build.version"
    if not version_file.is_file():
        return f"unknown ({engine_root})"
    try:
        version = json.loads(version_file.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return f"unparsable ({version_file})"
    major = version.get("MajorVersion")
    minor = version.get("MinorVersion")
    patch = version.get("PatchVersion")
    changelist = version.get("Changelist")
    return f"{major}.{minor}.{patch} changelist {changelist}"


def check_repository_has_no_generated_outputs() -> None:
    for path in ROOT.rglob("*"):
        if ".git" in path.parts:
            continue
        relative = path.relative_to(ROOT)
        if relative.parts and relative.parts[0] in {"build", "deps"}:
            continue
        if any(part in REPOSITORY_GENERATED_PARTS for part in relative.parts):
            fail(f"generated Unreal output must not be committed in the repo tree: {relative}")


def require_file(path: Path, package_dir: Path) -> None:
    if not path.is_file():
        fail(f"BuildPlugin package is missing required file: {path.relative_to(package_dir)}")


def load_package_descriptor(package_dir: Path) -> dict:
    descriptor_path = package_dir / "Nozzle.uplugin"
    require_file(descriptor_path, package_dir)
    try:
        descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        fail(f"BuildPlugin package has invalid Nozzle.uplugin JSON: {error}")
    if not isinstance(descriptor, dict):
        fail("BuildPlugin package Nozzle.uplugin JSON root must be an object")
    if descriptor.get("FriendlyName") != "Nozzle":
        fail("BuildPlugin package Nozzle.uplugin FriendlyName must be Nozzle")
    return descriptor


def check_no_forbidden_package_paths(package_dir: Path) -> None:
    if (package_dir / "Native").exists():
        fail("BuildPlugin package must not contain package-root Native/; the native bridge belongs under Source/Nozzle/Private/Native")
    if (package_dir / "deps").exists():
        fail("BuildPlugin package must not contain the development deps/ submodule")
    for path in package_dir.rglob("*"):
        relative = path.relative_to(package_dir)
        if ".git" in relative.parts:
            fail(f"BuildPlugin package must not contain git metadata: {relative}")
        if any(part in PACKAGE_FORBIDDEN_PARTS for part in relative.parts):
            fail(f"BuildPlugin package must not contain generated Unreal scratch directory: {relative}")


def assert_source_layout(package_dir: Path) -> None:
    for relative in SOURCE_LAYOUT_REQUIRED_FILES:
        require_file(package_dir / relative, package_dir)


def assert_binary_layout(package_dir: Path, target_platform: str | None) -> None:
    if (package_dir / "Source").exists():
        fail("binary-only BuildPlugin layout was requested, but Source/ is present")
    binaries = package_dir / "Binaries"
    if not binaries.is_dir():
        fail("binary-only BuildPlugin layout was requested, but Binaries/ is missing")
    if target_platform is not None:
        target_binaries = binaries / target_platform
        if not target_binaries.is_dir():
            fail(f"binary-only BuildPlugin layout for {target_platform} is missing Binaries/{target_platform}/")
        if not any(path.is_file() for path in target_binaries.rglob("*")):
            fail(f"binary-only BuildPlugin layout for {target_platform} has an empty Binaries/{target_platform}/ directory")
        return
    if not any(path.is_file() for path in binaries.rglob("*")):
        fail("binary-only BuildPlugin layout has an empty Binaries/ directory")


def assert_package_shape(package_dir: Path, expected_layout: str, target_platform: str | None) -> None:
    if not package_dir.is_dir():
        fail(f"BuildPlugin did not create the package directory: {package_dir}")

    load_package_descriptor(package_dir)
    check_no_forbidden_package_paths(package_dir)

    if expected_layout == "source":
        assert_source_layout(package_dir)
        print("BuildPlugin package assertion: source layout")
    elif expected_layout == "binary":
        assert_binary_layout(package_dir, target_platform)
        if target_platform is None:
            print("BuildPlugin package assertion: binary layout")
        else:
            print(f"BuildPlugin package assertion: binary layout for {target_platform}")
    else:
        if (package_dir / "Source").is_dir():
            assert_source_layout(package_dir)
            print("BuildPlugin package assertion: source layout")
        else:
            assert_binary_layout(package_dir, target_platform)
            if target_platform is None:
                print("BuildPlugin package assertion: binary layout")
            else:
                print(f"BuildPlugin package assertion: binary layout for {target_platform}")


def print_package_tree(package_dir: Path) -> None:
    files = sorted(path for path in package_dir.rglob("*") if path.is_file())
    print(f"BuildPlugin package tree: {package_dir}")
    for path in files[:200]:
        print(path.relative_to(package_dir).as_posix())
    if 200 < len(files):
        print(f"... {len(files) - 200} more files")
    print(f"BuildPlugin package file count: {len(files)}")


def make_runuat_command(runuat: Path, arguments: list[str]) -> list[str]:
    if runuat.suffix.lower() in {".bat", ".cmd"}:
        return ["cmd", "/c", str(runuat), *arguments]
    return [str(runuat), *arguments]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runuat", type=Path, help="Path to RunUAT.sh, RunUAT.command, or RunUAT.bat")
    parser.add_argument("--engine-root", type=Path, help="Path to Unreal Engine root or its parent containing Engine/")
    parser.add_argument("--package", type=Path, default=ROOT / "build" / "BuildPlugin" / "Nozzle")
    parser.add_argument("--target-platform", choices=("Win64", "Mac"), help="Pass -TargetPlatforms=<value> to RunUAT BuildPlugin and validate target-specific binary output")
    parser.add_argument("--no-rocket", action="store_true", help="Do not pass -Rocket to BuildPlugin")
    parser.add_argument("--expect-layout", choices=("auto", "source", "binary"), default="auto")
    parser.add_argument("--assert-package-only", action="store_true", help="Skip RunUAT and assert an existing package directory")
    args = parser.parse_args()

    if not PLUGIN_DESCRIPTOR.is_file():
        fail(f"plugin descriptor is missing: {PLUGIN_DESCRIPTOR}")

    package_dir = args.package if args.package.is_absolute() else ROOT / args.package
    if args.assert_package_only:
        assert_package_shape(package_dir, args.expect_layout, args.target_platform)
        print_package_tree(package_dir)
        return

    check_repository_has_no_generated_outputs()
    runuat = resolve_runuat(args)
    package_dir.parent.mkdir(parents=True, exist_ok=True)
    print(f"RunUAT: {runuat}")
    print(f"Unreal Engine version: {read_engine_version(runuat)}")
    print(f"Unreal BuildPlugin target platform: {args.target_platform or 'RunUAT default'}")
    print(f"nozzle-unreal SHA: {subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()}")
    print(f"nozzle core SHA: {subprocess.check_output(['git', '-C', 'deps/nozzle', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()}")

    runuat_arguments = [
        "BuildPlugin",
        f"-plugin={PLUGIN_DESCRIPTOR}",
        f"-package={package_dir}",
    ]
    if args.target_platform is not None:
        runuat_arguments.append(f"-TargetPlatforms={args.target_platform}")
    if not args.no_rocket:
        runuat_arguments.append("-Rocket")
    command = make_runuat_command(runuat, runuat_arguments)

    print("Command:")
    print(" ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)

    assert_package_shape(package_dir, args.expect_layout, args.target_platform)
    print_package_tree(package_dir)


if __name__ == "__main__":
    main()
