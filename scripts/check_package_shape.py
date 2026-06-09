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

LOCAL_OUTPUT_PARTS = {
    "build",
    "cmake-build-debug",
    "cmake-build-release",
}

REQUIRED_FILES = [
    "README.md",
    "LICENSE",
    "THIRD-PARTY-NOTICES.md",
    ".gitmodules",
    ".github/workflows/static-package-shape.yml",
    ".github/workflows/unreal-buildplugin.yml",
    "CMakeLists.txt",
    "scripts/check_build_plugin_package_assertions.py",
    "scripts/check_buildplugin_module_dependencies.py",
    "scripts/check_buildplugin_module_dependency_assertions.py",
    "scripts/check_native_staging_contract_assertions.py",
    "scripts/check_package_shape.py",
    "scripts/check_native_staging.py",
    "scripts/package_source.py",
    "scripts/run_build_plugin.py",
    "scripts/stage_native_nozzle.py",
    "Nozzle/Nozzle.uplugin",
    "Nozzle/Config/FilterPlugin.ini",
    "Nozzle/Source/Nozzle/Nozzle.Build.cs",
    "Nozzle/Source/Nozzle/Public/NozzleDiagnostics.h",
    "Nozzle/Source/Nozzle/Public/NozzleRuntimeBlueprintLibrary.h",
    "Nozzle/Source/Nozzle/Public/NozzleReceiverComponent.h",
    "Nozzle/Source/Nozzle/Public/NozzleRuntimeModule.h",
    "Nozzle/Source/Nozzle/Public/NozzleSenderComponent.h",
    "Nozzle/Source/Nozzle/Private/NozzleNativeBridge.h",
    "Nozzle/Source/Nozzle/Private/NozzleNativeBridge.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleNativeBridgeMetal.mm",
    "Nozzle/Source/Nozzle/Private/NozzleReceiverComponent.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleRuntimeBlueprintLibrary.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleRuntimeModule.cpp",
    "Nozzle/Source/Nozzle/Private/NozzleSenderComponent.cpp",
    "Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.h",
    "Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.cpp",
    "Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge_compile_check.cpp",
    "Nozzle/Source/NozzleEditor/NozzleEditor.Build.cs",
    "Nozzle/Source/NozzleEditor/Public/NozzleEditorModule.h",
    "Nozzle/Source/NozzleEditor/Private/NozzleEditorModule.cpp",
    "Nozzle/Source/ThirdParty/NozzleCore/NozzleCore.Build.cs",
    "Nozzle/ThirdParty/nozzle/README.md",
    "Nozzle/Resources/README.md",
    "Samples/NozzleSmoke/NozzleSmoke.uproject",
    "Samples/NozzleSmoke/Config/DefaultEngine.ini",
    "Samples/NozzleSmoke/Config/DefaultGame.ini",
    "Samples/NozzleSmoke/Content/NozzleSmoke/NozzleSmokeReceiverMaterial.uasset",
    "Samples/NozzleSmoke/Source/NozzleSmoke.Target.cs",
    "Samples/NozzleSmoke/Source/NozzleSmokeEditor.Target.cs",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.Build.cs",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.cpp",
    "Samples/NozzleSmoke/Source/NozzleSmoke/NozzleSmoke.h",
    "docs/phase-0-feasibility.md",
    "docs/macos-metal-smoke-protocol.md",
    "docs/runtime-smoke-matrix.md",
    "docs/win64-d3d11-smoke-protocol.md",
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


def forbid_text(path: Path, needle: str, label: str | None = None) -> None:
    require_file(path)
    text = path.read_text(encoding="utf-8")
    if needle in text:
        fail(f"{path.relative_to(ROOT)} must not contain {label or needle!r}")


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
        if any(part in LOCAL_OUTPUT_PARTS for part in relative.parts):
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
        if any(part in LOCAL_OUTPUT_PARTS for part in relative.parts):
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
        if module.get("PlatformAllowList") != ["Win64", "Mac"]:
            fail(f"module {name} PlatformAllowList must be exactly ['Win64', 'Mac']")


def check_sample_project() -> None:
    project = load_json(ROOT / "Samples" / "NozzleSmoke" / "NozzleSmoke.uproject")
    plugins = project.get("Plugins")
    if not isinstance(plugins, list) or not any(isinstance(plugin, dict) and plugin.get("Name") == "Nozzle" for plugin in plugins):
        fail("sample .uproject must enable the Nozzle plugin")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Config" / "DefaultEngine.ini", "DefaultGraphicsRHI_DX11")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Config" / "DefaultGame.ini", 'DirectoriesToAlwaysCook=(Path="/Game/NozzleSmoke")')
    require_file(ROOT / "Samples" / "NozzleSmoke" / "Content" / "NozzleSmoke" / "NozzleSmokeReceiverMaterial.uasset")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Source" / "NozzleSmoke" / "NozzleSmoke.cpp", "/Game/NozzleSmoke/NozzleSmokeReceiverMaterial.NozzleSmokeReceiverMaterial")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Source" / "NozzleSmoke" / "NozzleSmoke.cpp", "NozzleSmokePackagedReceiver")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "Source" / "NozzleSmoke" / "NozzleSmoke.cpp", "nozzle_frame_copy_to_native_texture")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "README.md", "not runtime evidence")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "README.md", "-NozzleSmokePackagedReceiver")
    require_text(ROOT / "Samples" / "NozzleSmoke" / "README.md", "NOZZLE_RECEIVER_SMOKE_RESULT packaged=1 row_status=PASS_CANDIDATE")



def check_native_include_path_resolution(runtime_build: Path) -> None:
    text = runtime_build.read_text(encoding="utf-8")
    required_expression = 'Path.Combine(ModuleDirectory, "Private", "Native")'
    if required_expression not in text:
        fail(f"{runtime_build.relative_to(ROOT)} must include the plugin-tree private Native bridge directory")
    if 'Path.Combine(RepositoryRoot, "Native")' in text or '"..", "..", ".."' in text:
        fail(f"{runtime_build.relative_to(ROOT)} must not include a repository-root Native bridge path")
    module_directory = PLUGIN_ROOT / "Source" / "Nozzle"
    resolved_header = (module_directory / "Private" / "Native" / "nozzle_unreal_native_bridge.h").resolve()
    expected_header = (PLUGIN_ROOT / "Source" / "Nozzle" / "Private" / "Native" / "nozzle_unreal_native_bridge.h").resolve()
    if resolved_header != expected_header:
        fail(f"Nozzle.Build.cs Native include path resolves to {resolved_header}, expected {expected_header}")
    if not resolved_header.is_file():
        fail(f"Nozzle.Build.cs Native include path does not contain nozzle_unreal_native_bridge.h: {resolved_header}")

def check_build_files() -> None:
    runtime_build = PLUGIN_ROOT / "Source" / "Nozzle" / "Nozzle.Build.cs"
    third_party_build = PLUGIN_ROOT / "Source" / "ThirdParty" / "NozzleCore" / "NozzleCore.Build.cs"
    require_text(runtime_build, "\"RHI\"")
    require_text(runtime_build, "\"RenderCore\"")
    require_text(runtime_build, "\"D3D11RHI\"")
    require_text(runtime_build, "\"MetalRHI\"")
    require_text(runtime_build, "AddEngineThirdPartyPrivateStaticDependencies(Target, \"DX11\")")
    require_text(runtime_build, "NOZZLE_UNREAL_PHASE0_RHI_D3D11=1")
    require_text(runtime_build, "NOZZLE_UNREAL_PHASE0_RHI_METAL=1")
    require_text(runtime_build, "NOZZLE_UNREAL_D3D11_RUNTIME=1")
    require_text(runtime_build, "NOZZLE_UNREAL_METAL_RUNTIME=1")
    require_text(runtime_build, "PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, \"Private\", \"Native\"))")
    check_native_include_path_resolution(runtime_build)
    require_text(third_party_build, "Type = ModuleType.External")
    require_text(third_party_build, "WITH_NOZZLE_CORE=0")
    require_text(third_party_build, "WITH_NOZZLE_CORE=1")
    require_text(third_party_build, "NozzleCore: WITH_NOZZLE_CORE=1")
    require_text(third_party_build, "NozzleCore: WITH_NOZZLE_CORE=0")
    require_text(third_party_build, "nozzle_c.h")
    require_text(third_party_build, "PublicDelayLoadDLLs.Add(\"nozzle.dll\")")
    require_text(third_party_build, "RuntimeDependencies.Add(RuntimeLibraryPath)")
    forbid_text(third_party_build, "RuntimeDependencies.Add(RuntimeDependencyTargetPath, RuntimeLibraryPath)")
    require_text(third_party_build, "libnozzle.dylib")
    require_text(PLUGIN_ROOT / "Config" / "FilterPlugin.ini", "/ThirdParty/nozzle/...")
    require_text(PLUGIN_ROOT / "ThirdParty" / "nozzle" / "README.md", "intentionally empty")



def require_no_drain_in_readiness(path: Path, function_name: str) -> None:
    require_file(path)
    text = path.read_text(encoding="utf-8")
    marker = f"bool {function_name}"
    start = text.find(marker)
    if start < 0:
        fail(f"{path.relative_to(ROOT)} must contain {function_name}")
    next_function = text.find("\n}\n\n", start)
    if next_function < 0:
        fail(f"{path.relative_to(ROOT)} has an unparsable {function_name} body")
    body = text[start:next_function]
    if "DrainRenderThreadDiagnostics()" in body:
        fail(f"{path.relative_to(ROOT)} {function_name} must not drain render diagnostics before overwriting readiness diagnostics")

def check_runtime_api_skeleton() -> None:
    public_dir = PLUGIN_ROOT / "Source" / "Nozzle" / "Public"
    private_dir = PLUGIN_ROOT / "Source" / "Nozzle" / "Private"
    diagnostics = public_dir / "NozzleDiagnostics.h"
    sender = public_dir / "NozzleSenderComponent.h"
    receiver = public_dir / "NozzleReceiverComponent.h"
    blueprint = public_dir / "NozzleRuntimeBlueprintLibrary.h"
    bridge = private_dir / "NozzleNativeBridge.cpp"
    sender_impl = private_dir / "NozzleSenderComponent.cpp"
    receiver_impl = private_dir / "NozzleReceiverComponent.cpp"
    blueprint_impl = private_dir / "NozzleRuntimeBlueprintLibrary.cpp"

    require_text(diagnostics, "FNozzleRuntimeDiagnostics")
    require_text(diagnostics, "UnsupportedRHI")
    require_text(diagnostics, "bWithNozzleCore")
    require_text(diagnostics, "bMetalRHI")
    require_text(sender, "UNozzleSenderComponent")
    require_text(sender, "UTextureRenderTarget2D")
    require_text(sender, "TickComponent")
    require_text(sender, "TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe>")
    require_text(sender, "GetLastRenderDiagnostics")
    require_text(sender, "GetLastRenderSequence")
    require_text(receiver, "UNozzleReceiverComponent")
    require_text(receiver, "UTextureRenderTarget2D")
    require_text(receiver, "TickComponent")
    require_text(receiver, "TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe>")
    require_text(receiver, "GetLastRenderDiagnostics")
    require_text(receiver, "GetLastRenderSequence")
    require_text(blueprint, "GetNozzleRuntimeDiagnostics")
    require_text(blueprint, "EnumerateNozzleSenders")

    require_text(bridge, "GDynamicRHI")
    require_text(bridge, "FRHITexture::GetNativeResource")
    require_text(bridge, "nozzle_unreal_native_bridge.h")
    require_text(bridge, "CaptureNativeTextureAndDevice_RenderThread")
    require_text(bridge, "ID3D11Texture2D::GetDevice")
    require_text(bridge, "NozzleUnrealExtractMetalDeviceFromNativeTexture")
    require_text(bridge, "nozzle_sender_create_with_native_device")
    require_text(bridge, "nozzle_unreal_native::publish_d3d11_texture")
    require_text(bridge, "nozzle_unreal_native::copy_frame_to_d3d11_texture")
    require_text(bridge, "unsupported RHI")
    require_text(bridge, "Win64 D3D11 and macOS Metal")
    require_text(bridge, "D3D12 and Linux are not supported")
    require_text(bridge, "WITH_NOZZLE_CORE")
    require_text(private_dir / "NozzleNativeBridgeMetal.mm", "id<MTLTexture>")
    require_text(private_dir / "NozzleNativeBridgeMetal.mm", "[Texture device]")

    require_text(sender_impl, "WITH_NOZZLE_CORE")
    require_text(sender_impl, "CreateSenderForNativeDevice_RenderThread")
    require_text(sender_impl, "PublishNativeTexture_RenderThread")
    require_text(sender_impl, "ENQUEUE_RENDER_COMMAND")
    require_text(sender_impl, "TSharedPtr<FNozzleSenderRenderState, ESPMode::ThreadSafe>")
    require_text(sender_impl, "DrainRenderThreadDiagnostics")
    require_text(sender_impl, "bCancelRequested")
    require_text(sender_impl, "FlushRenderingCommands()")
    require_text(sender_impl, "StoreSenderRenderDiagnostics")
    require_text(sender_impl, "CompletedRenderSequence += 1")
    require_text(sender_impl, "LastRenderDiagnostics")
    require_text(sender_impl, "LastRenderSequence")
    require_text(sender_impl, "GetLastRenderDiagnostics")
    require_text(sender_impl, "GetLastRenderSequence")
    require_no_drain_in_readiness(sender_impl, "UNozzleSenderComponent::RefreshRuntimeReadiness")
    forbid_text(sender_impl, "UNozzleSenderComponent* Component = this", "raw component capture in render command")
    forbid_text(sender_impl, "nozzle_sender_create(&", "default-device sender creation")
    forbid_text(sender_impl, "nozzle_sender_publish_native_texture_ex", "direct native publish outside shared bridge")
    require_text(receiver_impl, "WITH_NOZZLE_CORE")
    require_text(receiver_impl, "CreateReceiverForBackend")
    require_text(receiver_impl, "CopyFrameToNativeTexture_RenderThread")
    require_text(receiver_impl, "ENQUEUE_RENDER_COMMAND")
    require_text(receiver_impl, "TSharedPtr<FNozzleReceiverRenderState, ESPMode::ThreadSafe>")
    require_text(receiver_impl, "DrainRenderThreadDiagnostics")
    require_text(receiver_impl, "bCancelRequested")
    require_text(receiver_impl, "FlushRenderingCommands()")
    require_text(receiver_impl, "StoreReceiverRenderDiagnostics")
    require_text(receiver_impl, "CompletedRenderSequence += 1")
    require_text(receiver_impl, "LastRenderDiagnostics")
    require_text(receiver_impl, "LastRenderSequence")
    require_text(receiver_impl, "GetLastRenderDiagnostics")
    require_text(receiver_impl, "GetLastRenderSequence")
    require_no_drain_in_readiness(receiver_impl, "UNozzleReceiverComponent::RefreshRuntimeReadiness")
    forbid_text(receiver_impl, "nozzle_receiver_create(&", "direct receiver creation outside shared bridge")
    forbid_text(receiver_impl, "nozzle_frame_copy_to_native_texture", "direct native copy outside shared bridge")
    require_text(blueprint_impl, "nozzle_enumerate_senders")


def check_native_bridge() -> None:
    cmake = ROOT / "CMakeLists.txt"
    native_dir = PLUGIN_ROOT / "Source" / "Nozzle" / "Private" / "Native"
    header = native_dir / "nozzle_unreal_native_bridge.h"
    implementation = native_dir / "nozzle_unreal_native_bridge.cpp"
    compile_check = native_dir / "nozzle_unreal_native_bridge_compile_check.cpp"

    require_text(cmake, "project(nozzle_unreal_native_bridge LANGUAGES CXX)")
    require_text(cmake, "add_library(nozzle_unreal_native_bridge OBJECT")
    require_text(cmake, "Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge.cpp")
    require_text(cmake, "Nozzle/Source/Nozzle/Private/Native/nozzle_unreal_native_bridge_compile_check.cpp")
    require_text(cmake, "NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE")
    require_text(cmake, "NOZZLE_UNREAL_NATIVE_TARGET_MACOS")
    require_text(cmake, "NOZZLE_UNREAL_NATIVE_METAL_RUNTIME")
    require_text(cmake, "deps/nozzle/include")

    require_text(header, "#include <nozzle/nozzle_c.h>")
    require_text(header, "runtime_diagnostics")
    require_text(header, "d3d11_device_view")
    require_text(header, "metal_device_view")
    require_text(header, "d3d11_texture_view")
    require_text(header, "metal_texture_view")
    require_text(header, "create_d3d11_sender")
    require_text(header, "create_metal_sender")
    require_text(header, "publish_d3d11_texture")
    require_text(header, "publish_metal_texture")
    require_text(header, "copy_frame_to_d3d11_texture")
    require_text(header, "copy_frame_to_metal_texture")

    require_text(header, "nozzle_sender_create_with_native_device")
    require_text(header, "nozzle_sender_publish_native_texture_ex")
    require_text(header, "nozzle_frame_copy_to_native_texture")
    require_text(header, "NOZZLE_UNREAL_NATIVE_TARGET_WIN64")
    require_text(header, "NOZZLE_UNREAL_NATIVE_TARGET_MACOS")
    require_text(header, "NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME")
    require_text(header, "NOZZLE_UNREAL_NATIVE_METAL_RUNTIME")
    require_text(header, "NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE")
    require_text(header, "#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE")
    require_text(header, "typedef struct NozzleSender NozzleSender;")
    require_text(header, "#define NOZZLE_FALLBACK_NONE 0u")
    require_text(header, "not Unreal Engine runtime evidence")
    require_text(implementation, "header-inline")
    require_text(implementation, "Unreal component does not actually call")

    require_text(compile_check, "make_runtime_diagnostics")
    require_text(compile_check, "NOZZLE_BACKEND_D3D11")
    require_text(compile_check, "NOZZLE_BACKEND_METAL")
    require_text(compile_check, "NOZZLE_FORMAT_BGRA8_UNORM")


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
        if any(part in LOCAL_OUTPUT_PARTS for part in relative.parts):
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
            fail(f"possible false D3D12/Linux or macOS runtime support claim in {relative}:{line_number}: {line.strip()}")


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
    require_text(readme, "macOS + Metal")
    require_text(readme, "plugin-tree native bridge")
    require_text(readme, "BuildPlugin/UHT is still not proven")
    require_text(readme, "Do not use the default command as native-link evidence")
    require_text(readme, "--require <Mac|Win64> --inspect-deps")
    require_text(readme, "native_staging_mode")
    require_text(readme, "build_native_payload")
    require_text(readme, "scripts/stage_native_nozzle.py")
    require_text(readme, "post-BuildPlugin packaged-payload check")
    require_text(readme, "NozzleCore: WITH_NOZZLE_CORE=1")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "What is not proven yet")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "package-root `Native/` is absent")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "default placeholder validation is not native-link evidence")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "native_staging_mode=require")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "build_native_payload=true")
    require_text(ROOT / "docs" / "phase-0-feasibility.md", "build/BuildPlugin/Nozzle-<target>/ThirdParty/nozzle")
    require_text(ROOT / "docs" / "runtime-smoke-matrix.md", "Unreal sender -> nozzle-mixer")
    require_text(ROOT / "docs" / "runtime-smoke-matrix.md", "nozzle-viewer -> Unreal receiver/material")
    require_text(ROOT / "docs" / "runtime-smoke-matrix.md", "nozzle-mixer -> Unreal receiver/material")
    forbid_text(ROOT / "docs" / "runtime-smoke-matrix.md", "nozzle-viewer/nozzle-mixer -> Unreal receiver/material", "ambiguous combined receiver smoke row")
    require_text(ROOT / "docs" / "runtime-smoke-matrix.md", "MISSING")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "Texture sharing/backing outcome")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "non-null IOSurface proof and an IOSurface ID")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "explicitly named Metal frame-to-target path")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "MTLPixelFormat")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "storageMode")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "command buffer completion handler or wait")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "Multi-frame sequence evidence")
    require_text(ROOT / "docs" / "macos-metal-smoke-protocol.md", "non-symmetric source image or generated test pattern")
    require_text(ROOT / "docs" / "win64-d3d11-smoke-protocol.md", "Last render diagnostics sequence")
    require_text(ROOT / "THIRD-PARTY-NOTICES.md", "does not ship third-party binaries")


def check_workflow() -> None:
    workflow = ROOT / ".github" / "workflows" / "static-package-shape.yml"
    buildplugin_workflow = ROOT / ".github" / "workflows" / "unreal-buildplugin.yml"
    require_text(workflow, "python3 scripts/check_package_shape.py")
    require_text(workflow, "python3 scripts/check_native_staging.py")
    require_text(workflow, "python3 scripts/check_native_staging_contract_assertions.py")
    require_text(workflow, "python3 scripts/check_build_plugin_package_assertions.py")
    require_text(workflow, "python3 scripts/check_buildplugin_module_dependency_assertions.py")
    require_text(workflow, "python3 scripts/package_source.py")
    require_text(workflow, "cmake -S . -B build/native-ci")
    require_text(workflow, "NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=OFF")
    require_text(workflow, "windows-latest")
    require_text(workflow, "macos-latest")
    require_text(workflow, "linux-unsupported")
    text = workflow.read_text(encoding="utf-8")
    if "RunUAT" in text or "BuildPlugin" in text:
        fail("static workflow must not pretend to run Unreal BuildPlugin")
    require_text(buildplugin_workflow, "workflow_dispatch")
    require_text(buildplugin_workflow, "runner_labels_json")
    require_text(buildplugin_workflow, '["self-hosted","Windows","Unreal"]')
    require_text(buildplugin_workflow, "fromJSON(inputs.runner_labels_json)")
    require_text(buildplugin_workflow, "scripts/run_build_plugin.py")
    require_text(buildplugin_workflow, "native_staging_mode")
    require_text(buildplugin_workflow, "build_native_payload")
    require_text(buildplugin_workflow, "python3 scripts/stage_native_nozzle.py --platform")
    require_text(buildplugin_workflow, "python3 scripts/check_native_staging.py --require")
    require_text(buildplugin_workflow, "--staged-root \"build/BuildPlugin/Nozzle-${{ inputs.target_platform }}/ThirdParty/nozzle\"")
    require_text(buildplugin_workflow, "python3 scripts/check_buildplugin_module_dependencies.py")
    require_text(buildplugin_workflow, "--package-root \"build/BuildPlugin/Nozzle-${{ inputs.target_platform }}\"")
    require_text(buildplugin_workflow, "--inspect-deps")
    require_text(buildplugin_workflow, "--target-platform")
    require_text(buildplugin_workflow, "${{ inputs.target_platform }}")
    forbid_text(buildplugin_workflow, "actions/setup-python@v5", "hosted Python setup on self-hosted Unreal runner")
    require_text(buildplugin_workflow, "python3 scripts/check_package_shape.py")
    require_text(buildplugin_workflow, "python3 scripts/check_native_staging.py")
    require_text(buildplugin_workflow, "python3")
    require_text(buildplugin_workflow, "Verify RunUAT path exists")
    require_text(buildplugin_workflow, "Upload asserted BuildPlugin package")
    require_text(buildplugin_workflow, "if-no-files-found: error")
    buildplugin_script = ROOT / "scripts" / "run_build_plugin.py"
    require_text(buildplugin_script, "-TargetPlatforms=")
    require_text(buildplugin_script, "Binaries/{target_platform}/")
    require_text(buildplugin_script, "--allow-runuat-default-target")
    require_text(buildplugin_script, "--allow-source-only-artifact")
    require_text(buildplugin_script, "source-only BuildPlugin artifact assertion passed; this is diagnostic and is not #142 acceptance evidence")
    require_text(buildplugin_script, "target-pinned BuildPlugin evidence requires non-empty Binaries/{target_platform}/")
    require_text(buildplugin_script, "RunUAT BuildPlugin evidence must pass --target-platform")


def check_working_tree_shape() -> None:
    for relative in REQUIRED_FILES:
        require_file(ROOT / relative)
    check_no_generated_dirs()
    check_no_suppressed_failures(ROOT.rglob("*"))
    check_uplugin()
    check_sample_project()
    check_build_files()
    check_runtime_api_skeleton()
    check_native_bridge()
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
