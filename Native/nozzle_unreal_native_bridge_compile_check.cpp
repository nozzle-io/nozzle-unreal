#include "nozzle_unreal_native_bridge.h"

namespace {

int compile_check() noexcept {
    constexpr bool compiled_d3d11_runtime = NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME != 0;
    constexpr bool compiled_metal_runtime = NOZZLE_UNREAL_NATIVE_METAL_RUNTIME != 0;

    const nozzle_unreal_native::runtime_diagnostics diagnostics = nozzle_unreal_native::make_runtime_diagnostics(
        compiled_d3d11_runtime,
        compiled_metal_runtime
    );
    const NozzleNativeDevice d3d11_native_device = nozzle_unreal_native::make_d3d11_native_device({reinterpret_cast<void *>(1), reinterpret_cast<void *>(2)});
    const NozzleNativeDevice metal_native_device = nozzle_unreal_native::make_metal_native_device({reinterpret_cast<void *>(3)});
    const NozzleSenderDesc sender_desc = nozzle_unreal_native::make_sender_desc("UnrealNozzleSender", "Unreal", 3);
    const NozzleReceiverDesc receiver_desc = nozzle_unreal_native::make_receiver_desc("UnrealNozzleSender", "Unreal");
    const nozzle_unreal_native::d3d11_texture_view d3d11_texture_view{reinterpret_cast<void *>(4), 320, 240, NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM};
    const nozzle_unreal_native::metal_texture_view metal_texture_view{reinterpret_cast<void *>(5), 320, 240, NOZZLE_FORMAT_BGRA8_UNORM, NOZZLE_FORMAT_BGRA8_UNORM};

    int failures = 0;
    failures += d3d11_native_device.backend == NOZZLE_BACKEND_D3D11 ? 0 : 1;
    failures += d3d11_native_device.device != nullptr ? 0 : 1;
    failures += d3d11_native_device.context != nullptr ? 0 : 1;
    failures += metal_native_device.backend == NOZZLE_BACKEND_METAL ? 0 : 1;
    failures += metal_native_device.device != nullptr ? 0 : 1;
    failures += metal_native_device.context == nullptr ? 0 : 1;
    failures += sender_desc.fallback_flags == NOZZLE_FALLBACK_NONE ? 0 : 1;
    failures += sender_desc.fallback_flags_valid != 0 ? 0 : 1;
    failures += receiver_desc.receive_mode == NOZZLE_RECEIVE_LATEST_ONLY ? 0 : 1;
    failures += d3d11_texture_view.storage_format == NOZZLE_FORMAT_BGRA8_UNORM ? 0 : 1;
    failures += metal_texture_view.storage_format == NOZZLE_FORMAT_BGRA8_UNORM ? 0 : 1;

    if(compiled_d3d11_runtime) {
        failures += diagnostics.backend_type == nozzle_unreal_native::native_backend::d3d11 ? 0 : 1;
        failures += diagnostics.selected_rhi_is_d3d11 ? 0 : 1;
    } else if(compiled_metal_runtime) {
        failures += diagnostics.backend_type == nozzle_unreal_native::native_backend::metal ? 0 : 1;
        failures += diagnostics.selected_rhi_is_metal ? 0 : 1;
    } else {
        failures += diagnostics.backend_type == nozzle_unreal_native::native_backend::unsupported ? 0 : 1;
        failures += diagnostics.can_use_runtime ? 1 : 0;
    }

    return failures;
}

} // namespace

int nozzle_unreal_native_bridge_compile_check_anchor = compile_check();
