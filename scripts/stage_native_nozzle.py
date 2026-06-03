#!/usr/bin/env python3
"""Build and stage native nozzle payloads for Unreal BuildPlugin evidence."""

from __future__ import annotations

import argparse
import hashlib
import platform as host_platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEPS_NOZZLE = ROOT / "deps" / "nozzle"
STAGED_ROOT = ROOT / "Nozzle" / "ThirdParty" / "nozzle"


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run(command: list[str]) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        fail(f"missing build output: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    print(f"staged {destination.relative_to(ROOT)} size={destination.stat().st_size} sha256={sha256_file(destination)}")


def require_deps_nozzle() -> None:
    if not (DEPS_NOZZLE / "CMakeLists.txt").is_file():
        fail("deps/nozzle is missing; checkout must use recursive submodules")
    if not (DEPS_NOZZLE / "libs" / "plog" / "include" / "plog" / "Log.h").is_file():
        fail("deps/nozzle/libs/plog is missing; checkout must initialize nested submodules")


def stage_mac(build_root: Path) -> None:
    if host_platform.system() != "Darwin":
        fail("Mac native staging must run on macOS")

    install_root = build_root / "install"
    configure_command = [
        "cmake",
        "-S",
        str(DEPS_NOZZLE),
        "-B",
        str(build_root),
        "-DBUILD_SHARED_LIBS=ON",
        "-DNOZZLE_BUILD_EXAMPLES=OFF",
        "-DNOZZLE_BUILD_TESTS=OFF",
        "-DNOZZLE_INSTALL=ON",
        "-DNOZZLE_STRICT_NO_EXCEPTIONS=ON",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64",
        f"-DCMAKE_INSTALL_PREFIX={install_root}",
    ]
    run(configure_command)
    run(["cmake", "--build", str(build_root), "--target", "install", "--config", "Release"])

    copy_file(install_root / "include" / "nozzle" / "nozzle_c.h", STAGED_ROOT / "include" / "nozzle" / "nozzle_c.h")
    copy_file(install_root / "lib" / "libnozzle.dylib", STAGED_ROOT / "lib" / "Mac" / "libnozzle.dylib")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("Mac",), required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "stage-native-nozzle")
    args = parser.parse_args()

    require_deps_nozzle()
    build_root = args.build_dir / args.platform
    if not build_root.is_absolute():
        build_root = ROOT / build_root

    if args.platform == "Mac":
        stage_mac(build_root)
    else:
        fail(f"unsupported platform: {args.platform}")


if __name__ == "__main__":
    main()
