#!/usr/bin/env python3
"""Create a source-shape package for the nozzle Unreal scaffold.

This is not an Unreal BuildPlugin package and must not be described as one.
"""

from __future__ import annotations

import argparse
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXCLUDED_PARTS = {
    ".git",
    ".idea",
    ".vscode",
    ".vs",
    "Binaries",
    "DerivedDataCache",
    "deps",
    "Intermediate",
    "Saved",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
}

INCLUDED_ROOTS = [
    ".github",
    "CMakeLists.txt",
    "Native",
    "Nozzle",
    "Samples",
    "docs",
    "scripts",
    "README.md",
    "LICENSE",
    "THIRD-PARTY-NOTICES.md",
    ".gitignore",
    ".gitmodules",
]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def should_include(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    if any(part in EXCLUDED_PARTS for part in relative.parts):
        return False
    if path.is_dir():
        return False
    return True


def iter_files() -> list[Path]:
    files: list[Path] = []
    for entry in INCLUDED_ROOTS:
        path = ROOT / entry
        if not path.exists():
            fail(f"missing package input: {entry}")
        if path.is_file():
            files.append(path)
        else:
            files.extend(file for file in sorted(path.rglob("*")) if should_include(file))
    return sorted(files)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--root-name", default="nozzle-unreal-scaffold")
    args = parser.parse_args()

    output = args.output
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)

    root_name = args.root_name.strip().strip("/")
    if not root_name:
        fail("--root-name must not be empty")

    files = iter_files()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            relative = path.relative_to(ROOT).as_posix()
            archive.write(path, f"{root_name}/{relative}")

    print(f"wrote {output.relative_to(ROOT) if output.is_relative_to(ROOT) else output}")
    print(f"included {len(files)} files")


if __name__ == "__main__":
    main()
