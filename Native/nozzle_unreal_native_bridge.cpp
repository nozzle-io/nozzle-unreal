#include "nozzle_unreal_native_bridge.h"

namespace nozzle_unreal_native {

const char *supported_backend_name() noexcept {
    return "D3D11";
}

const char *unsupported_runtime_message() noexcept {
    return "native bridge is gated to Win64 D3D11 with staged nozzle core; Unreal Engine and UHT smoke remain unverified";
}

runtime_diagnostics make_runtime_diagnostics(bool selected_rhi_is_d3d11) noexcept {
    runtime_diagnostics diagnostics;
    diagnostics.with_nozzle_core = NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE != 0;
    diagnostics.target_win64 = NOZZLE_UNREAL_NATIVE_TARGET_WIN64 != 0;
    diagnostics.d3d11_runtime_enabled = NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME != 0;
    diagnostics.selected_rhi_is_d3d11 = selected_rhi_is_d3d11;
    diagnostics.backend = selected_rhi_is_d3d11 ? supported_backend_name() : "unsupported";

    if(!diagnostics.target_win64 || !diagnostics.d3d11_runtime_enabled) {
        diagnostics.state = runtime_state::unsupported_rhi;
        diagnostics.message = "native bridge compile target is not Win64 D3D11";
        return diagnostics;
    }

    if(!diagnostics.selected_rhi_is_d3d11) {
        diagnostics.state = runtime_state::unsupported_rhi;
        diagnostics.message = "selected Unreal RHI is not D3D11";
        return diagnostics;
    }

    if(!diagnostics.with_nozzle_core) {
        diagnostics.state = runtime_state::unavailable;
        diagnostics.message = "NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=0: nozzle C API calls are compiled out";
        return diagnostics;
    }

    diagnostics.state = runtime_state::ready;
    diagnostics.can_use_runtime = true;
    diagnostics.message = "Win64 D3D11 guard and nozzle C API path are compile-enabled; this is not Unreal Engine runtime evidence";
    return diagnostics;
}

NozzleNativeDevice make_native_device(d3d11_device_view device_view) noexcept {
    NozzleNativeDevice native_device{};
    native_device.backend = NOZZLE_BACKEND_D3D11;
    native_device.device = device_view.device;
    native_device.context = device_view.context;
    return native_device;
}

NozzleSenderDesc make_sender_desc(const char *sender_name, const char *application_name, std::uint32_t ring_buffer_size) noexcept {
    NozzleSenderDesc desc{};
    desc.name = sender_name;
    desc.application_name = application_name;
    desc.ring_buffer_size = ring_buffer_size;
    desc.allow_format_fallback = 0;
    desc.fallback_flags = NOZZLE_FALLBACK_NONE;
    desc.fallback_flags_valid = 1;
    return desc;
}

NozzleReceiverDesc make_receiver_desc(const char *sender_name, const char *application_name) noexcept {
    NozzleReceiverDesc desc{};
    desc.name = sender_name;
    desc.application_name = application_name;
    desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;
    return desc;
}

NozzleErrorCode create_d3d11_sender(
    const NozzleSenderDesc *sender_desc,
    d3d11_device_view device_view,
    bool selected_rhi_is_d3d11,
    NozzleSender **out_sender
) noexcept {
    if(out_sender == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }
    *out_sender = nullptr;

    const runtime_diagnostics diagnostics = make_runtime_diagnostics(selected_rhi_is_d3d11);
    if(!diagnostics.can_use_runtime) {
        return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    }

    if(sender_desc == nullptr || sender_desc->name == nullptr || device_view.device == nullptr || device_view.context == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    const NozzleNativeDevice native_device = make_native_device(device_view);
    return nozzle_sender_create_with_native_device(sender_desc, &native_device, out_sender);
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

NozzleErrorCode create_receiver(
    const NozzleReceiverDesc *receiver_desc,
    bool selected_rhi_is_d3d11,
    NozzleReceiver **out_receiver
) noexcept {
    if(out_receiver == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }
    *out_receiver = nullptr;

    const runtime_diagnostics diagnostics = make_runtime_diagnostics(selected_rhi_is_d3d11);
    if(!diagnostics.can_use_runtime) {
        return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    }

    if(receiver_desc == nullptr || receiver_desc->name == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    return nozzle_receiver_create(receiver_desc, out_receiver);
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

NozzleErrorCode publish_d3d11_texture(NozzleSender *sender, d3d11_texture_view texture_view) noexcept {
    if(sender == nullptr || texture_view.native_texture == nullptr || texture_view.width == 0 || texture_view.height == 0) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    return nozzle_sender_publish_native_texture_ex(
        sender,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format,
        texture_view.semantic_format
    );
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

NozzleErrorCode copy_frame_to_d3d11_texture(NozzleFrame *frame, d3d11_texture_view texture_view) noexcept {
    if(frame == nullptr || texture_view.native_texture == nullptr || texture_view.width == 0 || texture_view.height == 0) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    return nozzle_frame_copy_to_native_texture(
        frame,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format
    );
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

} // namespace nozzle_unreal_native
