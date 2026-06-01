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

GENERATED_PARTS = {
    "Binaries",
    "DerivedDataCache",
    "Intermediate",
    "Saved",
}


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
        if any(part in GENERATED_PARTS for part in relative.parts):
            fail(f"generated Unreal output must not be committed in the repo tree: {relative}")


def print_package_tree(package_dir: Path) -> None:
    files = sorted(path for path in package_dir.rglob("*") if path.is_file())
    print(f"BuildPlugin package tree: {package_dir}")
    for path in files[:200]:
        print(path.relative_to(package_dir).as_posix())
    if 200 < len(files):
        print(f"... {len(files) - 200} more files")
    print(f"BuildPlugin package file count: {len(files)}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runuat", type=Path, help="Path to RunUAT.sh, RunUAT.command, or RunUAT.bat")
    parser.add_argument("--engine-root", type=Path, help="Path to Unreal Engine root or its parent containing Engine/")
    parser.add_argument("--package", type=Path, default=ROOT / "build" / "BuildPlugin" / "Nozzle")
    parser.add_argument("--no-rocket", action="store_true", help="Do not pass -Rocket to BuildPlugin")
    args = parser.parse_args()

    if not PLUGIN_DESCRIPTOR.is_file():
        fail(f"plugin descriptor is missing: {PLUGIN_DESCRIPTOR}")

    check_repository_has_no_generated_outputs()
    runuat = resolve_runuat(args)
    package_dir = args.package if args.package.is_absolute() else ROOT / args.package
    package_dir.parent.mkdir(parents=True, exist_ok=True)

    print(f"RunUAT: {runuat}")
    print(f"Unreal Engine version: {read_engine_version(runuat)}")
    print(f"nozzle-unreal SHA: {subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()}")
    print(f"nozzle core SHA: {subprocess.check_output(['git', '-C', 'deps/nozzle', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()}")

    command = [
        str(runuat),
        "BuildPlugin",
        f"-plugin={PLUGIN_DESCRIPTOR}",
        f"-package={package_dir}",
    ]
    if not args.no_rocket:
        command.append("-Rocket")

    print("Command:")
    print(" ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)

    if not package_dir.is_dir():
        fail(f"BuildPlugin did not create the package directory: {package_dir}")
    print_package_tree(package_dir)


if __name__ == "__main__":
    main()
