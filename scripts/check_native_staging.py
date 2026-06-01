#!/usr/bin/env python3
"""Validate the staged nozzle native payload contract for the Unreal plugin."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGED_ROOT = ROOT / "Nozzle" / "ThirdParty" / "nozzle"
HEADER = STAGED_ROOT / "include" / "nozzle" / "nozzle_c.h"


@dataclass(frozen=True)
class PlatformContract:
    name: str
    link_library: Path
    runtime_library: Path
    dependency_tool: str
    dependency_command: tuple[str, ...]

    @property
    def required_files(self) -> tuple[Path, ...]:
        if self.link_library == self.runtime_library:
            return (HEADER, self.link_library)
        return (HEADER, self.link_library, self.runtime_library)


CONTRACTS = {
    "Win64": PlatformContract(
        name="Win64",
        link_library=STAGED_ROOT / "lib" / "Win64" / "nozzle.lib",
        runtime_library=STAGED_ROOT / "bin" / "Win64" / "nozzle.dll",
        dependency_tool="dumpbin",
        dependency_command=("dumpbin", "/DEPENDENTS"),
    ),
    "Mac": PlatformContract(
        name="Mac",
        link_library=STAGED_ROOT / "lib" / "Mac" / "libnozzle.dylib",
        runtime_library=STAGED_ROOT / "lib" / "Mac" / "libnozzle.dylib",
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


def present_files(contract: PlatformContract) -> list[Path]:
    return [path for path in contract.required_files if path.exists()]


def validate_contract(contract: PlatformContract, require_present: bool, inspect_deps: bool) -> None:
    present = present_files(contract)
    missing = [path for path in contract.required_files if not path.exists()]

    if not present:
        if require_present:
            fail(f"{contract.name} staging is required but no required files are present under {relative(STAGED_ROOT)}")
        print(f"{contract.name}: missing all staged files; WITH_NOZZLE_CORE must remain 0")
        return

    if missing:
        fail(
            f"{contract.name} staging is partial; this would create a false WITH_NOZZLE_CORE boundary. "
            f"missing: {', '.join(relative(path) for path in missing)}"
        )

    print(f"{contract.name}: complete staged payload")
    for path in contract.required_files:
        print(f"  {relative(path)}")

    if inspect_deps:
        tool = shutil.which(contract.dependency_tool)
        if tool is None:
            fail(f"{contract.name} dependency inspection requires {contract.dependency_tool}, but it was not found")
        command = [tool, *contract.dependency_command[1:], str(contract.runtime_library)]
        print(f"{contract.name} dependency inspection: {' '.join(command)}")
        subprocess.run(command, cwd=ROOT, check=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require", choices=sorted(CONTRACTS), action="append", default=[])
    parser.add_argument("--require-all", action="store_true")
    parser.add_argument("--inspect-deps", action="store_true")
    args = parser.parse_args()

    required = set(args.require)
    if args.require_all:
        required.update(CONTRACTS)

    for name in sorted(CONTRACTS):
        validate_contract(CONTRACTS[name], require_present=name in required, inspect_deps=args.inspect_deps and name in required)


if __name__ == "__main__":
    main()
