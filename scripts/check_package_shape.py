#!/usr/bin/env python3
"""Static shape checks for nozzle-unreal.

This intentionally does not compile Unreal code. It makes that CI boundary
explicit instead of producing a misleading green engine-build claim.
"""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
PLUGIN_ROOT = ROOT / "Nozzle"

GENERATED_PARTS = {
    "Binaries",
    "DerivedDataCache",
    "Intermediate",
    "Saved",
}

DEV_SUBMODULE_PARTS = {
    "deps",
}

REQUIRED_FILES = [
    "README.md",
    "LICENSE",
    "THIRD-PARTY-NOTICES.md",
    ".gitmodules",
    ".github/workflows/static-package-shape.yml",
    "scripts/check_package_shape.py",
    "scripts/package_source.py",
    "Nozzle/Nozzle.uplugin",
    "Nozzle/Source/Nozzle/Nozzle.Build.cs",
    "Nozzle/Source/Nozzle/Public/NozzleDiagnostics.h",
    "Nozzle/Source/Nozzle/Public/NozzleRuntimeBlueprintLibrary.h",
    "Nozzle/Source/Nozzle/Public/NozzleReceiverComponent.h",
    "Nozzle/Source/Nozzle/Public/NozzleRuntimeModule.h",
    "Nozzle/Source/Nozzle/Public/NozzleSenderComponent.h",
    "Nozzle/Source/Nozzle/Private/NozzleD3D11Bridge.h",
    "Nozzle/Source/Nozzle/Private/NozzleD3D11Bridge.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleReceiverComponent.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleRuntimeBlueprintLibrary.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleRuntimeModule.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleSenderComponent.cpp",
    "Nozzle/Source/NozzleEditor/NozzleEditor.Build.cs",
    "Nozzle/Source/NozzleEditor/Public/NozzleEditorModule.h",
    "Nozzle/Source/NozzleEditor/Private/NozzleEditorModule.cpp",
    "Nozzle/Source/ThirdParty/NozzleCore/NozzleCore.Build.cs",
    "Nozzle/ThirdParty/nozzle/README.md",
    "Nozzle/Resources/README.md",
    "Samples/NozzleSmoke/NozzleSmoke.uproject",
    "Samples/NozzleSmoke/Config/DefaultEngine.ini",
    "Samples/NozzleSmoke/Source/NozzleSmoke.Target.cs",
    "Samples/NozzleSmoke/Source/NozzleSmokeEditor.Target.cs",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.Build.cs",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.cpp",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.h",
    "docs/phase-0-feasibility.md",
    "docs/runtime-smoke-matrix.md",
]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def require_file(path: Path) -> None:
    if not path.is_file():
        fail(f"required file is missing: {path.relative_to(ROOT)}")


def require_text(path: Path, needle: str, label: str | None = None) -> None:
    require_file(path)
    text = path.read_text(encoding="utf-8")
    if needle not in text:
        fail(f"{path.relative_to(ROOT)} must contain {label or needle!r}")


def load_json(path: Path) -> dict:
    require_file(path)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        fail(f"invalid JSON in {path.relative_to(ROOT)}: {error}")
    if not isinstance(data, dict):
        fail(f"JSON root must be an object: {path.relative_to(ROOT)}")
    return data


def check_no_generated_dirs() -> None:
    for path in ROOT.rglob("*"):
        if ".git" in path.parts:
            continue
        relative = path.relative_to(ROOT)
        if any(part in DEV_SUBMODULE_PARTS for part in relative.parts):
            continue
        if any(part in GENERATED_PARTS for part in relative.parts):
            fail(f"generated Unreal directory must not be committed: {relative}")


def check_no_suppressed_failures(paths: Iterable[Path]) -> None:
    for path in paths:
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT)
        if any(part in DEV_SUBMODULE_PARTS for part in relative.parts):
            continue
        if path.suffix not in {".yml", ".yaml", ".sh", ".py", ".md"}:
            continue
        text = path.read_text(encoding="utf-8")
        suppressed_failure_token = "||" + " true"
        if suppressed_failure_token in text:
            fail(f"failure suppression is forbidden in {path.relative_to(ROOT)}")


def check_uplugin() -> None:
    descriptor = load_json(PLUGIN_ROOT / "Nozzle.uplugin")
    if descriptor.get("FileVersion") != 3:
        fail("Nozzle.uplugin FileVersion must be 3")
    if descriptor.get("FriendlyName") != "Nozzle":
        fail("Nozzle.uplugin FriendlyName must be Nozzle")
    if descriptor.get("CanContainContent") is not False:
        fail("Phase 0 scaffold must not claim plugin content support")

    modules = descriptor.get("Modules")
    if not isinstance(modules, list):
        fail("Nozzle.uplugin Modules must be a list")
    by_name = {module.get("Name"): module for module in modules if isinstance(module, dict)}
    for name, module_type in {"Nozzle": "Runtime", "NozzleEditor": "Editor"}.items():
        module = by_name.get(name)
        if not isinstance(module, dict):
            fail(f"missing module descriptor: {name}")
        if module.get("Type") != module_type:
            fail(f"module {name} Type must be {module_type}")
        if module.get("LoadingPhase") != "Default":
            fail(f"module {name} LoadingPhase must be Default")
        if module.get("PlatformAllowList") != ["Win64"]:
            fail(f"module {name} PlatformAllowList must be exactly ['Win64']")


def check_sample_project() -> None:
    project = load_json(ROOT / "Samples" / "NozzleSmoke" / "NozzleSmoke.uproject")
    plugins = project.get("Plugins")
    if not isinstance(plugins, list) or not any(isinstance(plugin, dict) and plugin.get("Name") == "Nozzle" for plugin in plugins):
        fail("sample .uproject must enable the Nozzle plugin")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Config" / "DefaultEngine.ini", "DefaultGraphicsRHI_DX11")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "README.md", "not runtime evidence")


def check_build_files() -> None:
    runtime_build = PLUGIN_ROOT / "Source" / "Nozzle" / "Nozzle.Build.cs"
    third_party_build = PLUGIN_ROOT / "Source" / "ThirdParty" / "NozzleCore" / "NozzleCore.Build.cs"
    require_text(runtime_build, "\"RHI\"")
    require_text(runtime_build, "\"RenderCore\"")
    require_text(runtime_build, "\"D3D11RHI\"")
    require_text(runtime_build, "AddEngineThirdPartyPrivateStaticDependencies(Target, \"DX11\")")
    require_text(runtime_build, "NOZZLE_UNREAL_PHASE0_RHI_D3D11=1")
    require_text(runtime_build, "NOZZLE_UNREAL_D3D11_RUNTIME=1")
    require_text(third_party_build, "Type = ModuleType.External")
    require_text(third_party_build, "WITH_NOZZLE_CORE=0")
    require_text(third_party_build, "WITH_NOZZLE_CORE=1")
    require_text(third_party_build, "nozzle_c.h")
    require_text(PLUGIN_ROOT / "ThirdParty" / "nozzle" / "README.md", "intentionally empty")


def check_runtime_api_skeleton() -> None:
    public_dir = PLUGIN_ROOT / "Source" / "Nozzle" / "Public"
    private_dir = PLUGIN_ROOT / "Source" / "Nozzle" / "Private"
    diagnostics = public_dir / "NozzleDiagnostics.h"
    sender = public_dir / "NozzleSenderComponent.h"
    receiver = public_dir / "NozzleReceiverComponent.h"
    blueprint = public_dir / "NozzleRuntimeBlueprintLibrary.h"
    bridge = private_dir / "NozzleD3D11Bridge.cpp"
    sender_impl = private_dir / "NozzleSenderComponent.cpp"
    receiver_impl = private_dir / "NozzleReceiverComponent.cpp"
    blueprint_impl = private_dir / "NozzleRuntimeBlueprintLibrary.cpp"

    require_text(diagnostics, "FNozzleRuntimeDiagnostics")
    require_text(diagnostics, "UnsupportedRHI")
    require_text(diagnostics, "bWithNozzleCore")
    require_text(sender, "UNozzleSenderComponent")
    require_text(sender, "UTextureRenderTarget2D")
    require_text(receiver, "UNozzleReceiverComponent")
    require_text(receiver, "UTextureRenderTarget2D")
    require_text(blueprint, "GetNozzleRuntimeDiagnostics")
    require_text(blueprint, "EnumerateNozzleSenders")

    require_text(bridge, "GDynamicRHI")
    require_text(bridge, "FRHITexture::GetNativeResource")
    require_text(bridge, "unsupported RHI")
    require_text(bridge, "Win64 D3D11 only")
    require_text(bridge, "D3D12, macOS, and Linux are not supported")
    require_text(bridge, "WITH_NOZZLE_CORE")

    require_text(sender_impl, "WITH_NOZZLE_CORE")
    require_text(sender_impl, "nozzle_sender_create")
    require_text(sender_impl, "nozzle_sender_publish_native_texture_ex")
    require_text(sender_impl, "NOZZLE_FORMAT_BGRA8_UNORM")
    require_text(sender_impl, "ENQUEUE_RENDER_COMMAND")
    require_text(receiver_impl, "WITH_NOZZLE_CORE")
    require_text(receiver_impl, "nozzle_receiver_create")
    require_text(receiver_impl, "nozzle_frame_copy_to_native_texture")
    require_text(receiver_impl, "NOZZLE_FORMAT_BGRA8_UNORM")
    require_text(receiver_impl, "ENQUEUE_RENDER_COMMAND")
    require_text(blueprint_impl, "nozzle_enumerate_senders")


def check_no_false_support_claims() -> None:
    checked_suffixes = {".cs", ".cpp", ".h", ".md", ".ini", ".uplugin", ".uproject", ".py", ".yml", ".yaml"}
    positive_words = ("support", "supported", "supports", "validated", "working", "runtime")
    negative_words = ("not", "unsupported", "no ", "false", "do not", "without", "blocked", "reject")
    watched_tokens = ("D3D12", "macOS", "Linux")
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix not in checked_suffixes:
            continue
        relative = path.relative_to(ROOT)
        if any(part in DEV_SUBMODULE_PARTS for part in relative.parts):
            continue
        text = path.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), start=1):
            lowered = line.lower()
            if not any(token.lower() in lowered for token in watched_tokens):
                continue
            if not any(word in lowered for word in positive_words):
                continue
            if any(word in lowered for word in negative_words):
                continue
            fail(f"possible false D3D12/macOS/Linux support claim in {relative}:{line_number}: {line.strip()}")


def check_dev_submodule() -> None:
    gitmodules = ROOT / ".gitmodules"
    require_text(gitmodules, "path = deps/nozzle")
    require_text(gitmodules, "url = git@github.com:nozzle-io/nozzle.git")
    if any(child.name == "nozzle" for child in ROOT.iterdir()):
        fail("root nozzle submodule would collide with Nozzle/ on case-insensitive macOS filesystems; use deps/nozzle")


def check_docs() -> None:
    readme = ROOT / "README.md"
    require_text(readme, "does **not** claim Unreal runtime support")
    require_text(readme, "does not invoke Unreal Engine")
    require_text(readme, "Win64 + D3D11")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "What is not proven yet")
    require_text(ROOT / "docs" / "runtime-smoke-matrix.md", "MISSING")
    require_text(ROOT / "THIRD-PARTY-NOTICES.md", "does not ship third-party binaries")


def check_workflow() -> None:
    workflow = ROOT / ".github" / "workflows" / "static-package-shape.yml"
    require_text(workflow, "python3 scripts/check_package_shape.py")
    require_text(workflow, "python3 scripts/package_source.py")
    text = workflow.read_text(encoding="utf-8")
    if "RunUAT" in text or "BuildPlugin" in text:
        fail("static workflow must not pretend to run Unreal BuildPlugin")


def check_working_tree_shape() -> None:
    for relative in REQUIRED_FILES:
        require_file(ROOT / relative)
    check_no_generated_dirs()
    check_no_suppressed_failures(ROOT.rglob("*"))
    check_uplugin()
    check_sample_project()
    check_build_files()
    check_runtime_api_skeleton()
    check_no_false_support_claims()
    check_dev_submodule()
    check_docs()
    check_workflow()


def check_package(package_path: Path) -> None:
    if not package_path.is_file():
        fail(f"package is missing: {package_path}")
    with zipfile.ZipFile(package_path) as archive:
        names = archive.namelist()
    if not names:
        fail("package is empty")
    roots = {name.split("/", 1)[0] for name in names if name and not name.endswith("/")}
    if len(roots) != 1:
        fail(f"package must contain exactly one root directory, got {sorted(roots)}")
    root = next(iter(roots))
    required = [f"{root}/{relative}" for relative in REQUIRED_FILES]
    missing = [name for name in required if name not in names]
    if missing:
        fail("package is missing required entries: " + ", ".join(missing))
    for name in names:
        parts = Path(name).parts
        if any(part in GENERATED_PARTS or part == ".git" for part in parts):
            fail(f"package contains forbidden generated/git path: {name}")
    print(f"package shape passed: {package_path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, help="optional source package zip to validate")
    args = parser.parse_args()

    check_working_tree_shape()
    print("working tree shape passed")
    if args.package is not None:
        package_path = args.package if args.package.is_absolute() else ROOT / args.package
        check_package(package_path)


if __name__ == "__main__":
    main()
