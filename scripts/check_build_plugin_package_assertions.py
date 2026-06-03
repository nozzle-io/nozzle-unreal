#!/usr/bin/env python3
"""Behavior regressions for BuildPlugin package assertions.

This script does not require Unreal Engine. It creates fake BuildPlugin package
trees and verifies scripts/run_build_plugin.py --assert-package-only behavior via
subprocesses. The point is to test the actual package oracle, not static strings.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

from run_build_plugin import SOURCE_LAYOUT_REQUIRED_FILES  # noqa: E402


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def write_file(path: Path, text: str = "") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_descriptor(package_dir: Path) -> None:
    write_file(package_dir / "Nozzle.uplugin", '{"FriendlyName":"Nozzle"}\n')


def write_source_layout(package_dir: Path) -> None:
    for relative in SOURCE_LAYOUT_REQUIRED_FILES:
        path = package_dir / relative
        if relative == "Nozzle.uplugin":
            write_descriptor(package_dir)
        else:
            write_file(path, f"fixture for {relative}\n")


def write_binary(package_dir: Path, target_platform: str, filename: str) -> None:
    write_file(package_dir / "Binaries" / target_platform / filename, "binary fixture\n")


def run_assertion(package_dir: Path, *args: str) -> subprocess.CompletedProcess[str]:
    command = [
        sys.executable,
        str(SCRIPTS / "run_build_plugin.py"),
        "--assert-package-only",
        "--package",
        str(package_dir),
        *args,
    ]
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True)


def combined_output(result: subprocess.CompletedProcess[str]) -> str:
    return result.stdout + result.stderr


def require_success(result: subprocess.CompletedProcess[str], expected_text: str, label: str) -> None:
    output = combined_output(result)
    if result.returncode != 0:
        fail(f"{label} should pass, got exit {result.returncode}\n{output}")
    if expected_text not in output:
        fail(f"{label} output is missing {expected_text!r}\n{output}")
    print(f"PASS: {label}")


def require_failure(result: subprocess.CompletedProcess[str], expected_text: str, label: str) -> None:
    output = combined_output(result)
    if result.returncode == 0:
        fail(f"{label} should fail, got exit 0\n{output}")
    if expected_text not in output:
        fail(f"{label} output is missing {expected_text!r}\n{output}")
    print(f"PASS: {label}")


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="nozzle-unreal-buildplugin-assertions-") as temp_root:
        root = Path(temp_root)

        source_correct = root / "source-correct"
        write_source_layout(source_correct)
        write_binary(source_correct, "Win64", "Nozzle.dll")
        require_success(
            run_assertion(source_correct, "--expect-layout", "auto", "--target-platform", "Win64"),
            "BuildPlugin package assertion: source layout with target binary evidence for Win64",
            "source layout with matching Win64 binaries",
        )

        source_wrong = root / "source-wrong-target"
        write_source_layout(source_wrong)
        write_binary(source_wrong, "Mac", "Nozzle.dylib")
        require_failure(
            run_assertion(source_wrong, "--expect-layout", "auto", "--target-platform", "Win64"),
            "target-pinned BuildPlugin evidence requires non-empty Binaries/Win64/",
            "source layout with only wrong-target binaries",
        )

        source_missing = root / "source-missing-target"
        write_source_layout(source_missing)
        require_failure(
            run_assertion(source_missing, "--expect-layout", "auto", "--target-platform", "Win64"),
            "target-pinned BuildPlugin evidence requires non-empty Binaries/Win64/",
            "source layout without target binaries",
        )

        source_only_diagnostic = root / "source-only-diagnostic"
        shutil.copytree(source_missing, source_only_diagnostic)
        require_success(
            run_assertion(
                source_only_diagnostic,
                "--expect-layout",
                "auto",
                "--target-platform",
                "Win64",
                "--allow-source-only-artifact",
            ),
            "source-only BuildPlugin artifact assertion passed; this is diagnostic and is not #142 acceptance evidence",
            "source-only diagnostic mode",
        )

        binary_correct = root / "binary-correct"
        write_descriptor(binary_correct)
        write_binary(binary_correct, "Win64", "Nozzle.dll")
        require_success(
            run_assertion(binary_correct, "--expect-layout", "binary", "--target-platform", "Win64"),
            "BuildPlugin package assertion: binary layout for Win64",
            "binary-only layout with matching Win64 binaries",
        )

        binary_wrong = root / "binary-wrong-target"
        write_descriptor(binary_wrong)
        write_binary(binary_wrong, "Mac", "Nozzle.dylib")
        require_failure(
            run_assertion(binary_wrong, "--expect-layout", "binary", "--target-platform", "Win64"),
            "binary-only BuildPlugin layout for Win64 is missing Binaries/Win64/",
            "binary-only layout with only wrong-target binaries",
        )

    print("BuildPlugin package assertion regressions passed: 6 cases")


if __name__ == "__main__":
    main()
