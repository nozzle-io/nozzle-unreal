#!/usr/bin/env python3
"""Validate the staged nozzle native payload contract for the Unreal plugin."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STAGED_ROOT = ROOT / "Nozzle" / "ThirdParty" / "nozzle"

HEADER_TOKENS = (
    "typedef enum NozzleErrorCode",
    "NOZZLE_ERROR_INVALID_ARGUMENT",
    "NOZZLE_ERROR_UNSUPPORTED_BACKEND",
    "typedef enum NozzleBackendType",
    "NOZZLE_BACKEND_D3D11",
    "NOZZLE_BACKEND_METAL",
    "typedef enum NozzleTextureFormat",
    "NOZZLE_FORMAT_BGRA8_UNORM",
    "typedef struct NozzleNativeDevice",
    "nozzle_sender_create_with_native_device",
    "nozzle_sender_publish_native_texture_ex",
    "nozzle_frame_copy_to_native_texture",
)

MACOS_SYSTEM_PREFIXES = (
    "/usr/lib/",
    "/System/Library/",
)

MACOS_PACKAGE_LOAD_PREFIXES = (
    "@rpath/",
    "@loader_path/",
    "@executable_path/",
)

MACOS_BUILD_PATH_PREFIXES = (
    "/Users/",
    "/private/",
    "/tmp/",
    "/var/folders/",
)

WIN64_SYSTEM_DLL_PATTERNS = (
    "api-ms-win-*.dll",
    "ext-ms-*.dll",
    "kernel32.dll",
    "user32.dll",
    "advapi32.dll",
    "gdi32.dll",
    "shell32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "ws2_32.dll",
    "bcrypt.dll",
    "crypt32.dll",
    "d3d11.dll",
    "dxgi.dll",
    "opengl32.dll",
    "dbghelp.dll",
    "iphlpapi.dll",
    "msvcp*.dll",
    "vcruntime*.dll",
    "ucrtbase.dll",
    "ntdll.dll",
)


@dataclass(frozen=True)
class PlatformContract:
    name: str
    staged_root: Path
    header: Path
    link_library: Path
    runtime_library: Path
    dependency_tool: str
    dependency_command: tuple[str, ...]

    @property
    def required_files(self) -> tuple[Path, ...]:
        if self.link_library == self.runtime_library:
            return (self.header, self.link_library)
        return (self.header, self.link_library, self.runtime_library)


@dataclass(frozen=True)
class FileEvidence:
    path: Path
    size: int
    sha256: str


@dataclass(frozen=True)
class MacosDependencySlice:
    architecture: str | None
    dependencies: list[str]


def make_contracts(staged_root: Path = DEFAULT_STAGED_ROOT) -> dict[str, PlatformContract]:
    staged_root = staged_root.resolve()
    header = staged_root / "include" / "nozzle" / "nozzle_c.h"
    return {
        "Win64": PlatformContract(
            name="Win64",
            staged_root=staged_root,
            header=header,
            link_library=staged_root / "lib" / "Win64" / "nozzle.lib",
            runtime_library=staged_root / "bin" / "Win64" / "nozzle.dll",
            dependency_tool="dumpbin",
            dependency_command=("dumpbin", "/DEPENDENTS"),
        ),
        "Mac": PlatformContract(
            name="Mac",
            staged_root=staged_root,
            header=header,
            link_library=staged_root / "lib" / "Mac" / "libnozzle.dylib",
            runtime_library=staged_root / "lib" / "Mac" / "libnozzle.dylib",
            dependency_tool="otool",
            dependency_command=("otool", "-L"),
        ),
    }


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def relative(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_required_file(path: Path) -> FileEvidence | str | None:
    if not path.exists():
        return None
    if not path.is_file():
        return f"{relative(path)} exists but is not a file"
    size = path.stat().st_size
    if size <= 0:
        return f"{relative(path)} is empty"
    return FileEvidence(path=path, size=size, sha256=sha256_file(path))


def validate_header_surface(header: Path) -> list[str]:
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        return [f"{relative(header)} could not be read: {error}"]

    missing_tokens = [token for token in HEADER_TOKENS if token not in text]
    if missing_tokens:
        return [f"{relative(header)} is missing expected public C API tokens: {', '.join(missing_tokens)}"]
    return []


def print_file_evidence(evidence: FileEvidence) -> None:
    print(f"  {relative(evidence.path)} size={evidence.size} sha256={evidence.sha256}")


def parse_macos_otool_header(line: str) -> str | None | bool:
    if not line.endswith(":"):
        return False
    header = line[:-1]
    match = re.search(r" \(architecture ([^)]+)\)$", header)
    if match:
        return match.group(1)
    return None


def parse_macos_otool_output(output: str) -> list[MacosDependencySlice]:
    slices: list[MacosDependencySlice] = []
    current_architecture: str | None = None
    current_dependencies: list[str] = []
    saw_header = False

    def flush_current() -> None:
        nonlocal current_dependencies
        if current_dependencies:
            slices.append(MacosDependencySlice(architecture=current_architecture, dependencies=current_dependencies))
            current_dependencies = []

    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        parsed_header = parse_macos_otool_header(line)
        if parsed_header is not False:
            flush_current()
            current_architecture = parsed_header
            saw_header = True
            continue
        dependency = line.split(" (", 1)[0].strip()
        if dependency:
            if not saw_header and not slices:
                current_architecture = None
            current_dependencies.append(dependency)

    flush_current()
    return slices


def macos_is_system_dependency(dependency: str) -> bool:
    return dependency.startswith(MACOS_SYSTEM_PREFIXES)


def macos_is_package_load_path(dependency: str) -> bool:
    return dependency.startswith(MACOS_PACKAGE_LOAD_PREFIXES)


def staged_peer_for_dependency(contract: PlatformContract, dependency: str) -> Path:
    return contract.runtime_library.parent / Path(dependency).name


def describe_macos_slice(contract: PlatformContract, dependency_slice: MacosDependencySlice) -> str:
    if dependency_slice.architecture is None:
        return contract.name
    return f"{contract.name} {dependency_slice.architecture}"


def validate_macos_dependency_slice(contract: PlatformContract, dependency_slice: MacosDependencySlice) -> list[str]:
    errors: list[str] = []
    label = describe_macos_slice(contract, dependency_slice)
    dependencies = dependency_slice.dependencies
    if not dependencies:
        return [f"{label} dependency inspection returned no install name/dependencies"]

    install_name = dependencies[0]
    if not macos_is_package_load_path(install_name):
        errors.append(
            f"{label} install name must be package-loadable "
            f"({', '.join(MACOS_PACKAGE_LOAD_PREFIXES)}), got {install_name}"
        )
    if install_name.startswith(MACOS_BUILD_PATH_PREFIXES) or (install_name.startswith("/") and not macos_is_system_dependency(install_name)):
        errors.append(f"{label} install name points at a non-package absolute path: {install_name}")

    for dependency in dependencies[1:]:
        if dependency.startswith(MACOS_BUILD_PATH_PREFIXES):
            errors.append(f"{label} dependency points at a build-machine path: {dependency}")
            continue
        if macos_is_system_dependency(dependency):
            continue
        if dependency.startswith("/"):
            errors.append(f"{label} dependency is an unstaged absolute non-system path: {dependency}")
            continue
        if macos_is_package_load_path(dependency):
            staged_peer = staged_peer_for_dependency(contract, dependency)
            peer_status = inspect_required_file(staged_peer)
            if not isinstance(peer_status, FileEvidence):
                if peer_status is None:
                    peer_status = f"{relative(staged_peer)} is missing"
                errors.append(f"{label} dependency {dependency} is not staged beside the plugin: {peer_status}")
            continue
        errors.append(f"{label} dependency uses an undocumented load path: {dependency}")

    return errors


def validate_macos_dependencies(contract: PlatformContract, dependency_slices: list[MacosDependencySlice]) -> list[str]:
    if not dependency_slices:
        return [f"{contract.name} dependency inspection returned no architecture/install-name records"]
    errors: list[str] = []
    for dependency_slice in dependency_slices:
        errors.extend(validate_macos_dependency_slice(contract, dependency_slice))
    return errors


def parse_win64_dumpbin_output(output: str) -> list[str]:
    dependencies: list[str] = []
    in_dependencies = False
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not in_dependencies:
            if line.lower() == "image has the following dependencies:":
                in_dependencies = True
            continue
        if not line:
            if dependencies:
                break
            continue
        if line.lower().endswith(".dll"):
            dependencies.append(line)
    return dependencies


def win64_is_system_dependency(dependency: str) -> bool:
    lowered = Path(dependency).name.lower()
    return any(fnmatch.fnmatchcase(lowered, pattern) for pattern in WIN64_SYSTEM_DLL_PATTERNS)


def validate_win64_dependencies(contract: PlatformContract, dependencies: list[str]) -> list[str]:
    errors: list[str] = []
    if not dependencies:
        return [f"{contract.name} dependency inspection returned no DLL dependencies"]

    for dependency in dependencies:
        basename = Path(dependency).name
        if basename.lower() == contract.runtime_library.name.lower():
            continue
        if win64_is_system_dependency(basename):
            continue
        staged_peer = contract.runtime_library.parent / basename
        peer_status = inspect_required_file(staged_peer)
        if not isinstance(peer_status, FileEvidence):
            if peer_status is None:
                peer_status = f"{relative(staged_peer)} is missing"
            errors.append(f"{contract.name} dependency {basename} is not staged beside the plugin: {peer_status}")
    return errors


def inspect_dependencies(contract: PlatformContract) -> None:
    tool = shutil.which(contract.dependency_tool)
    if tool is None:
        fail(f"{contract.name} dependency inspection requires {contract.dependency_tool}, but it was not found")

    command = [tool, *contract.dependency_command[1:], str(contract.runtime_library)]
    print(f"{contract.name} dependency inspection: {' '.join(command)}")
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        fail(
            f"{contract.name} dependency inspection command failed with exit code {result.returncode}; "
            f"stdout={result.stdout.strip()!r}; stderr={result.stderr.strip()!r}"
        )

    if contract.name == "Mac":
        dependency_slices = parse_macos_otool_output(result.stdout)
        print(f"{contract.name} parsed dependencies:")
        for dependency_slice in dependency_slices:
            architecture = dependency_slice.architecture or "single"
            print(f"  architecture={architecture}")
            for dependency in dependency_slice.dependencies:
                print(f"    {dependency}")
        errors = validate_macos_dependencies(contract, dependency_slices)
    elif contract.name == "Win64":
        dependencies = parse_win64_dumpbin_output(result.stdout)
        print(f"{contract.name} parsed dependencies:")
        for dependency in dependencies:
            print(f"  {dependency}")
        errors = validate_win64_dependencies(contract, dependencies)
    else:
        errors = [f"unsupported dependency inspection platform: {contract.name}"]

    if errors:
        fail("; ".join(errors))


def validate_contract(contract: PlatformContract, require_present: bool, inspect_deps: bool) -> None:
    platform_files = tuple(dict.fromkeys((contract.link_library, contract.runtime_library)))
    platform_file_exists = any(path.exists() for path in platform_files)
    if not require_present and not platform_file_exists:
        print(f"{contract.name}: missing staged platform files; WITH_NOZZLE_CORE must remain 0")
        return

    statuses = {path: inspect_required_file(path) for path in contract.required_files}
    existing = [path for path in contract.required_files if statuses[path] is not None]

    if not existing:
        if require_present:
            fail(f"{contract.name} staging is required but no required files are present under {relative(contract.staged_root)}")
        print(f"{contract.name}: missing all staged files; WITH_NOZZLE_CORE must remain 0")
        return

    missing = [path for path in contract.required_files if statuses[path] is None]
    invalid = [status for status in statuses.values() if isinstance(status, str)]
    if missing:
        fail(
            f"{contract.name} staging is partial; this would create a false WITH_NOZZLE_CORE boundary. "
            f"missing: {', '.join(relative(path) for path in missing)}"
        )
    if invalid:
        fail(f"{contract.name} staging is invalid: {'; '.join(invalid)}")

    header_errors = validate_header_surface(contract.header)
    if header_errors:
        fail(f"{contract.name} staging header is invalid: {'; '.join(header_errors)}")

    print(f"{contract.name}: complete staged payload")
    for path in contract.required_files:
        status = statuses[path]
        if not isinstance(status, FileEvidence):
            fail(f"internal error: missing file evidence for {relative(path)}")
        print_file_evidence(status)

    if require_present and not inspect_deps:
        fail(f"{contract.name} require-mode native staging evidence must include --inspect-deps")

    if inspect_deps:
        inspect_dependencies(contract)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--staged-root", type=Path, default=DEFAULT_STAGED_ROOT)
    parser.add_argument("--require", choices=("Mac", "Win64"), action="append", default=[])
    parser.add_argument("--require-all", action="store_true")
    parser.add_argument("--inspect-deps", action="store_true")
    args = parser.parse_args()

    contracts = make_contracts(args.staged_root)
    required = set(args.require)
    if args.require_all:
        required.update(contracts)

    platform_files = {
        path
        for contract in contracts.values()
        for path in (contract.link_library, contract.runtime_library)
    }
    header = next(iter(contracts.values())).header
    header_status = inspect_required_file(header)
    if header_status is not None and not any(path.exists() for path in platform_files):
        if isinstance(header_status, str):
            fail(f"staging header is invalid: {header_status}")
        fail(
            f"staging is partial: {relative(header)} is staged without any platform link/runtime libraries; "
            "this would create a partial native staging boundary"
        )

    for name in sorted(contracts):
        validate_contract(contracts[name], require_present=name in required, inspect_deps=args.inspect_deps and name in required)


if __name__ == "__main__":
    main()
