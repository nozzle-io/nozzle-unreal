#!/usr/bin/env python3
"""Regression checks for scripts/check_native_staging.py."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

import check_native_staging as staging  # noqa: E402

CHECKER = SCRIPTS / "check_native_staging.py"

VALID_HEADER = """
typedef enum NozzleErrorCode { NOZZLE_ERROR_INVALID_ARGUMENT = 2, NOZZLE_ERROR_UNSUPPORTED_BACKEND = 3 } NozzleErrorCode;
typedef enum NozzleBackendType { NOZZLE_BACKEND_D3D11 = 1, NOZZLE_BACKEND_METAL = 2 } NozzleBackendType;
typedef enum NozzleTextureFormat { NOZZLE_FORMAT_BGRA8_UNORM = 5 } NozzleTextureFormat;
typedef struct NozzleNativeDevice { int backend; } NozzleNativeDevice;
NozzleErrorCode nozzle_sender_create_with_native_device(void);
NozzleErrorCode nozzle_sender_publish_native_texture_ex(void);
NozzleErrorCode nozzle_frame_copy_to_native_texture(void);
"""


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run_checker(staged_root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CHECKER), "--staged-root", str(staged_root), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def require_success(result: subprocess.CompletedProcess[str], label: str) -> None:
    if result.returncode != 0:
        fail(f"{label} failed unexpectedly\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}")


def require_failure(result: subprocess.CompletedProcess[str], label: str, needle: str) -> None:
    combined = result.stdout + result.stderr
    if result.returncode == 0:
        fail(f"{label} passed unexpectedly\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}")
    if needle not in combined:
        fail(f"{label} did not report {needle!r}\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}")


def write_file(path: Path, content: bytes | str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(content, bytes):
        path.write_bytes(content)
    else:
        path.write_text(content, encoding="utf-8")


def write_valid_mac_payload(staged_root: Path) -> None:
    write_file(staged_root / "include" / "nozzle" / "nozzle_c.h", VALID_HEADER)
    write_file(staged_root / "lib" / "Mac" / "libnozzle.dylib", b"fake-dylib")


def write_valid_win64_payload(staged_root: Path) -> None:
    write_file(staged_root / "include" / "nozzle" / "nozzle_c.h", VALID_HEADER)
    write_file(staged_root / "lib" / "Win64" / "nozzle.lib", b"fake-import-lib")
    write_file(staged_root / "bin" / "Win64" / "nozzle.dll", b"fake-dll")


def check_file_contracts() -> None:
    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        require_success(run_checker(staged_root), "empty placeholder default mode")

        write_file(staged_root / "include" / "nozzle" / "nozzle_c.h", VALID_HEADER)
        require_failure(run_checker(staged_root), "partial header-only staging", "staging is partial")

    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_mac_payload(staged_root)
        require_success(run_checker(staged_root), "complete Mac payload default mode")
        require_failure(run_checker(staged_root, "--require", "Mac"), "require mode without dependency inspection", "--inspect-deps")

    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_mac_payload(staged_root)
        (staged_root / "include" / "nozzle" / "nozzle_c.h").unlink()
        (staged_root / "include" / "nozzle" / "nozzle_c.h").mkdir(parents=True)
        require_failure(run_checker(staged_root), "directory pretending to be header", "not a file")

    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_mac_payload(staged_root)
        write_file(staged_root / "lib" / "Mac" / "libnozzle.dylib", b"")
        require_failure(run_checker(staged_root), "zero-byte dylib", "is empty")

    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_win64_payload(staged_root)
        write_file(staged_root / "include" / "nozzle" / "nozzle_c.h", "not the nozzle C API")
        require_failure(run_checker(staged_root), "wrong header surface", "missing expected public C API tokens")


def check_macos_dependency_parser() -> None:
    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_mac_payload(staged_root)
        contract = staging.make_contracts(staged_root)["Mac"]
        single_arch_output = f"""{contract.runtime_library}:
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.0.0)
    @rpath/libhelper.dylib (compatibility version 0.0.0, current version 0.0.0)
"""
        dependency_slices = staging.parse_macos_otool_output(single_arch_output)
        if len(dependency_slices) != 1 or dependency_slices[0].architecture is not None:
            fail(f"unexpected single-arch macOS dependency parse result: {dependency_slices}")
        errors = staging.validate_macos_dependencies(contract, dependency_slices)
        if not any("libhelper.dylib" in error for error in errors):
            fail(f"macOS unstaged @rpath dependency was not rejected: {errors}")
        write_file(staged_root / "lib" / "Mac" / "libhelper.dylib", b"fake-helper")
        errors = staging.validate_macos_dependencies(contract, dependency_slices)
        if errors:
            fail(f"macOS staged @rpath dependency was rejected: {errors}")

        universal_output = f"""{contract.runtime_library} (architecture x86_64):
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.0.0)
{contract.runtime_library} (architecture arm64):
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.0.0)
"""
        dependency_slices = staging.parse_macos_otool_output(universal_output)
        architectures = [dependency_slice.architecture for dependency_slice in dependency_slices]
        if architectures != ["x86_64", "arm64"]:
            fail(f"unexpected universal macOS architecture parse result: {architectures}")
        errors = staging.validate_macos_dependencies(contract, dependency_slices)
        if errors:
            fail(f"valid universal macOS dependencies were rejected: {errors}")

        bad_arm64_install_name = staging.parse_macos_otool_output(
            f"""{contract.runtime_library} (architecture x86_64):
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.0.0)
{contract.runtime_library} (architecture arm64):
    /Users/build/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
"""
        )
        errors = staging.validate_macos_dependencies(contract, bad_arm64_install_name)
        if not any("arm64 install name" in error for error in errors):
            fail(f"macOS per-architecture absolute install name was not rejected: {errors}")

        unstaged_arm64_dependency = staging.parse_macos_otool_output(
            f"""{contract.runtime_library} (architecture x86_64):
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1700.0.0)
{contract.runtime_library} (architecture arm64):
    @rpath/libnozzle.dylib (compatibility version 0.0.0, current version 0.0.0)
    @rpath/libmissing.dylib (compatibility version 0.0.0, current version 0.0.0)
"""
        )
        errors = staging.validate_macos_dependencies(contract, unstaged_arm64_dependency)
        if not any("arm64 dependency @rpath/libmissing.dylib" in error for error in errors):
            fail(f"macOS per-architecture unstaged dependency was not rejected: {errors}")


def check_win64_dependency_parser() -> None:
    with tempfile.TemporaryDirectory(prefix="nozzle-native-staging-") as directory:
        staged_root = Path(directory)
        write_valid_win64_payload(staged_root)
        contract = staging.make_contracts(staged_root)["Win64"]
        output = """
Dump of file nozzle.dll

File Type: DLL

  Image has the following dependencies:

    KERNEL32.dll
    d3d11.dll
    dxgi.dll
    VCRUNTIME140.dll
    helper.dll

  Summary
"""
        dependencies = staging.parse_win64_dumpbin_output(output)
        expected_dependencies = ["KERNEL32.dll", "d3d11.dll", "dxgi.dll", "VCRUNTIME140.dll", "helper.dll"]
        if dependencies != expected_dependencies:
            fail(f"unexpected Win64 dependency parse result: {dependencies}")
        errors = staging.validate_win64_dependencies(contract, dependencies)
        if not any("helper.dll" in error for error in errors):
            fail(f"Win64 unstaged plugin-private dependency was not rejected: {errors}")
        write_file(staged_root / "bin" / "Win64" / "helper.dll", b"fake-helper")
        errors = staging.validate_win64_dependencies(contract, dependencies)
        if errors:
            fail(f"Win64 staged plugin-private dependency was rejected: {errors}")


def main() -> None:
    check_file_contracts()
    check_macos_dependency_parser()
    check_win64_dependency_parser()
    print("native staging contract assertions passed")


if __name__ == "__main__":
    main()
