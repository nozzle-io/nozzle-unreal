#include "nozzle_unreal_native_bridge.h"

namespace {

int compile_check() noexcept {
    const nozzle_unreal_native::runtime_diagnostics diagnostics = nozzle_unreal_native::make_runtime_diagnostics(true);
    const NozzleNativeDevice native_device = nozzle_unreal_native::make_native_device({reinterpret_cast<void *>(1), reinterpret_cast<void *>(2)});
    const NozzleSenderDesc sender_desc = nozzle_unreal_native::make_sender_desc("UnrealNozzleSender", "Unreal", 3);
    const NozzleReceiverDesc receiver_desc = nozzle_unreal_native::make_receiver_desc("UnrealNozzleSender", "Unreal");
    const nozzle_unreal_native::d3d11_texture_view texture_view{reinterpret_cast<void *>(3), 320, 240, NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM};

    int failures = 0;
    failures += diagnostics.selected_rhi_is_d3d11 ? 0 : 1;
    failures += native_device.backend == NOZZLE_BACKEND_D3D11 ? 0 : 1;
    failures += native_device.device != nullptr ? 0 : 1;
    failures += native_device.context != nullptr ? 0 : 1;
    failures += sender_desc.fallback_flags == NOZZLE_FALLBACK_NONE ? 0 : 1;
    failures += sender_desc.fallback_flags_valid != 0 ? 0 : 1;
    failures += receiver_desc.receive_mode == NOZZLE_RECEIVE_LATEST_ONLY ? 0 : 1;
    failures += texture_view.storage_format == NOZZLE_FORMAT_BGRA8_UNORM ? 0 : 1;
    return failures;
}

} // namespace

int nozzle_unreal_native_bridge_compile_check_anchor = compile_check();
