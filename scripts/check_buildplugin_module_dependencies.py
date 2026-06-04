#!/usr/bin/env python3
"""Validate BuildPlugin module loader dependencies.

This is an artifact-level check. It inspects the binaries produced by Unreal
BuildPlugin and rejects module dylibs that still point at build-machine paths or
cannot plausibly resolve the staged nozzle runtime payload inside the plugin
package.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

from check_native_staging import (  # noqa: E402
    MACOS_BUILD_PATH_PREFIXES,
    MACOS_PACKAGE_LOAD_PREFIXES,
    MACOS_SYSTEM_PREFIXES,
    MacosDependencySlice,
    parse_macos_otool_output,
)

MAC_NOZZLE_MODULE = "UnrealEditor-Nozzle.dylib"
MAC_NOZZLE_DYLIB = Path("ThirdParty/nozzle/lib/Mac/libnozzle.dylib")


@dataclass(frozen=True)
class MacosModuleEvidence:
    module: Path
    dependency_slices: list[MacosDependencySlice]
    rpaths: list[str]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def relative(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def require_non_empty_file(path: Path, label: str, package_root: Path) -> None:
    if not path.is_file():
        fail(f"{label} is missing: {relative(path, package_root)}")
    if path.stat().st_size <= 0:
        fail(f"{label} is empty: {relative(path, package_root)}")


def parse_macos_otool_rpaths(output: str) -> list[str]:
    rpaths: list[str] = []
    expecting_path = False
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line == "cmd LC_RPATH":
            expecting_path = True
            continue
        if expecting_path:
            match = re.match(r"path (.+?) \(offset \d+\)$", line)
            if match:
                rpath = match.group(1)
                if rpath not in rpaths:
                    rpaths.append(rpath)
                expecting_path = False
    return rpaths


def macos_is_system_dependency(dependency: str) -> bool:
    return dependency.startswith(MACOS_SYSTEM_PREFIXES)


def macos_is_build_path(dependency: str) -> bool:
    return dependency.startswith(MACOS_BUILD_PATH_PREFIXES)


def macos_is_package_load_path(dependency: str) -> bool:
    return dependency.startswith(MACOS_PACKAGE_LOAD_PREFIXES)


def normalize_path(path: Path) -> Path:
    return Path(os.path.normpath(str(path)))


def resolve_loader_path(value: str, module: Path, package_root: Path) -> Path | None:
    if value == "@loader_path":
        return module.parent
    if value.startswith("@loader_path/"):
        return normalize_path(module.parent / value[len("@loader_path/") :])
    if value == "@executable_path":
        return package_root
    if value.startswith("@executable_path/"):
        return normalize_path(package_root / value[len("@executable_path/") :])
    if value.startswith("/"):
        return normalize_path(Path(value))
    return None


def resolve_dependency_candidates(dependency: str, module: Path, package_root: Path, rpaths: list[str]) -> list[Path]:
    if dependency.startswith("@rpath/"):
        suffix = dependency[len("@rpath/") :]
        candidates: list[Path] = []
        for rpath in rpaths:
            resolved_rpath = resolve_loader_path(rpath, module, package_root)
            if resolved_rpath is not None:
                candidates.append(normalize_path(resolved_rpath / suffix))
        return candidates

    resolved = resolve_loader_path(dependency, module, package_root)
    if resolved is None:
        return []
    return [resolved]


def is_libnozzle_dependency(dependency: str) -> bool:
    return Path(dependency).name == "libnozzle.dylib"


def validate_dependency_entry(
    package_root: Path,
    module: Path,
    rpaths: list[str],
    dependency: str,
    runtime_module: bool,
) -> list[str]:
    errors: list[str] = []
    module_label = relative(module, package_root)
    staged_lib = normalize_path(package_root / MAC_NOZZLE_DYLIB)

    if macos_is_build_path(dependency):
        return [f"{module_label} dependency points at a build-machine path: {dependency}"]
    if dependency.startswith("/") and not macos_is_system_dependency(dependency):
        return [f"{module_label} dependency is an unstaged absolute non-system path: {dependency}"]

    if not is_libnozzle_dependency(dependency):
        return errors

    if not runtime_module:
        errors.append(f"{module_label} unexpectedly links libnozzle.dylib; only {MAC_NOZZLE_MODULE} should carry the native dependency")

    if not macos_is_package_load_path(dependency):
        errors.append(
            f"{module_label} libnozzle dependency must be package-loadable "
            f"({', '.join(MACOS_PACKAGE_LOAD_PREFIXES)}), got {dependency}"
        )
        return errors

    candidates = resolve_dependency_candidates(dependency, module, package_root, rpaths)
    if not candidates:
        errors.append(f"{module_label} libnozzle dependency {dependency} has no resolvable loader-path candidate")
        return errors

    if staged_lib not in candidates:
        rendered = ", ".join(str(candidate) for candidate in candidates)
        errors.append(
            f"{module_label} libnozzle dependency {dependency} does not resolve to packaged "
            f"{MAC_NOZZLE_DYLIB.as_posix()}; candidates: {rendered}"
        )
        return errors

    if not staged_lib.is_file() or staged_lib.stat().st_size <= 0:
        errors.append(f"{module_label} libnozzle dependency resolves to missing/empty packaged dylib: {MAC_NOZZLE_DYLIB.as_posix()}")

    return errors


def validate_macos_module_evidence(package_root: Path, evidence: MacosModuleEvidence) -> list[str]:
    errors: list[str] = []
    runtime_module = evidence.module.name == MAC_NOZZLE_MODULE
    module_label = relative(evidence.module, package_root)
    saw_libnozzle = False

    if not evidence.dependency_slices:
        return [f"{module_label} dependency inspection returned no architecture/install-name records"]

    for dependency_slice in evidence.dependency_slices:
        architecture = dependency_slice.architecture or "single"
        if not dependency_slice.dependencies:
            errors.append(f"{module_label} architecture={architecture} has no install name/dependencies")
            continue
        for dependency in dependency_slice.dependencies:
            if is_libnozzle_dependency(dependency):
                saw_libnozzle = True
            errors.extend(validate_dependency_entry(package_root, evidence.module, evidence.rpaths, dependency, runtime_module))

    if runtime_module and not saw_libnozzle:
        errors.append(f"{module_label} must link packaged libnozzle.dylib when WITH_NOZZLE_CORE=1")

    return errors


def run_otool(args: list[str], subject: Path) -> str:
    tool = shutil.which("otool")
    if tool is None:
        fail("Mac module dependency inspection requires otool, but it was not found")
    command = [tool, *args, str(subject)]
    print(f"Mac module inspection: {' '.join(command)}")
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        fail(
            f"otool command failed for {subject} with exit code {result.returncode}; "
            f"stdout={result.stdout.strip()!r}; stderr={result.stderr.strip()!r}"
        )
    return result.stdout


def inspect_macos_module(module: Path, package_root: Path) -> MacosModuleEvidence:
    dependencies_output = run_otool(["-L"], module)
    rpath_output = run_otool(["-l"], module)
    dependency_slices = parse_macos_otool_output(dependencies_output)
    rpaths = parse_macos_otool_rpaths(rpath_output)

    print(f"Mac module parsed dependencies: {relative(module, package_root)}")
    for dependency_slice in dependency_slices:
        architecture = dependency_slice.architecture or "single"
        print(f"  architecture={architecture}")
        for dependency in dependency_slice.dependencies:
            print(f"    {dependency}")
    print(f"Mac module parsed rpaths: {relative(module, package_root)}")
    for rpath in rpaths:
        print(f"  {rpath}")

    return MacosModuleEvidence(module=module, dependency_slices=dependency_slices, rpaths=rpaths)


def check_macos_package(package_root: Path) -> None:
    binaries_dir = package_root / "Binaries" / "Mac"
    if not binaries_dir.is_dir():
        fail(f"Mac BuildPlugin package is missing Binaries/Mac/: {package_root}")

    staged_runtime = package_root / MAC_NOZZLE_DYLIB
    require_non_empty_file(staged_runtime, "packaged Mac native dylib", package_root)

    runtime_module = binaries_dir / MAC_NOZZLE_MODULE
    require_non_empty_file(runtime_module, "Mac Nozzle module", package_root)

    modules = sorted(binaries_dir.glob("*.dylib"))
    if not modules:
        fail(f"Mac BuildPlugin package has no module dylibs under {relative(binaries_dir, package_root)}")

    errors: list[str] = []
    for module in modules:
        if module.stat().st_size <= 0:
            errors.append(f"{relative(module, package_root)} is empty")
            continue
        evidence = inspect_macos_module(module, package_root)
        errors.extend(validate_macos_module_evidence(package_root, evidence))

    if errors:
        fail("; ".join(errors))

    print(f"Mac BuildPlugin module dependency check passed: {len(modules)} module dylib(s)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--target-platform", choices=("Mac",), required=True)
    args = parser.parse_args()

    package_root = args.package_root.resolve()
    if not package_root.is_dir():
        fail(f"BuildPlugin package root does not exist: {package_root}")

    if args.target_platform == "Mac":
        check_macos_package(package_root)


if __name__ == "__main__":
    main()
