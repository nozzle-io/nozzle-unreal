#pragma once

#include <cstdint>

#include <nozzle/nozzle_c.h>

#ifndef NOZZLE_UNREAL_NATIVE_TARGET_WIN64
#define NOZZLE_UNREAL_NATIVE_TARGET_WIN64 0
#endif

#ifndef NOZZLE_UNREAL_NATIVE_TARGET_MACOS
#define NOZZLE_UNREAL_NATIVE_TARGET_MACOS 0
#endif

#ifndef NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME
#define NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME 0
#endif

#ifndef NOZZLE_UNREAL_NATIVE_METAL_RUNTIME
#define NOZZLE_UNREAL_NATIVE_METAL_RUNTIME 0
#endif

#ifndef NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
#define NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE 0
#endif

namespace nozzle_unreal_native {

enum class native_backend {
    unsupported,
    d3d11,
    metal
};

enum class runtime_state {
    unavailable,
    unsupported_rhi,
    ready,
    error
};

struct runtime_diagnostics {
    runtime_state state = runtime_state::unavailable;
    bool with_nozzle_core = false;
    bool target_win64 = false;
    bool target_macos = false;
    bool d3d11_runtime_enabled = false;
    bool metal_runtime_enabled = false;
    bool selected_rhi_is_d3d11 = false;
    bool selected_rhi_is_metal = false;
    bool can_use_runtime = false;
    native_backend backend_type = native_backend::unsupported;
    const char *backend = "unsupported";
    const char *message = "not evaluated";
};

struct d3d11_device_view {
    void *device = nullptr;
    void *context = nullptr;
};

struct metal_device_view {
    void *device = nullptr;
};

struct d3d11_texture_view {
    void *native_texture = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    NozzleTextureFormat storage_format = NOZZLE_FORMAT_BGRA8_UNORM;
    NozzleTextureFormat semantic_format = NOZZLE_FORMAT_BGRA8_UNORM;
};

struct metal_texture_view {
    void *native_texture = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    NozzleTextureFormat storage_format = NOZZLE_FORMAT_BGRA8_UNORM;
    NozzleTextureFormat semantic_format = NOZZLE_FORMAT_BGRA8_UNORM;
};

inline bool can_use_d3d11(bool selected_rhi_is_d3d11) noexcept {
    return NOZZLE_UNREAL_NATIVE_TARGET_WIN64 != 0
        && NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME != 0
        && selected_rhi_is_d3d11;
}

inline bool can_use_metal(bool selected_rhi_is_metal) noexcept {
    return NOZZLE_UNREAL_NATIVE_TARGET_MACOS != 0
        && NOZZLE_UNREAL_NATIVE_METAL_RUNTIME != 0
        && selected_rhi_is_metal;
}

inline const char *supported_backend_name() noexcept {
#if NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME
    return "D3D11";
#elif NOZZLE_UNREAL_NATIVE_METAL_RUNTIME
    return "Metal";
#else
    return "unsupported";
#endif
}

inline const char *unsupported_runtime_message() noexcept {
    return "native bridge is gated to the platform backend compiled by CI: Win64 D3D11, macOS Metal, or explicit unsupported guard; Unreal Engine and UHT smoke remain unverified";
}

inline runtime_diagnostics make_runtime_diagnostics(bool selected_rhi_is_d3d11, bool selected_rhi_is_metal) noexcept {
    runtime_diagnostics diagnostics;
    diagnostics.with_nozzle_core = NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE != 0;
    diagnostics.target_win64 = NOZZLE_UNREAL_NATIVE_TARGET_WIN64 != 0;
    diagnostics.target_macos = NOZZLE_UNREAL_NATIVE_TARGET_MACOS != 0;
    diagnostics.d3d11_runtime_enabled = NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME != 0;
    diagnostics.metal_runtime_enabled = NOZZLE_UNREAL_NATIVE_METAL_RUNTIME != 0;
    diagnostics.selected_rhi_is_d3d11 = selected_rhi_is_d3d11;
    diagnostics.selected_rhi_is_metal = selected_rhi_is_metal;

    if(can_use_d3d11(selected_rhi_is_d3d11)) {
        diagnostics.backend_type = native_backend::d3d11;
        diagnostics.backend = "D3D11";
    } else if(can_use_metal(selected_rhi_is_metal)) {
        diagnostics.backend_type = native_backend::metal;
        diagnostics.backend = "Metal";
    } else {
        diagnostics.state = runtime_state::unsupported_rhi;
        diagnostics.backend_type = native_backend::unsupported;
        diagnostics.backend = "unsupported";
        diagnostics.message = unsupported_runtime_message();
        return diagnostics;
    }

    if(!diagnostics.with_nozzle_core) {
        diagnostics.state = runtime_state::unavailable;
        diagnostics.message = "NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=0: nozzle C API calls are compiled out";
        return diagnostics;
    }

    diagnostics.state = runtime_state::ready;
    diagnostics.can_use_runtime = true;
    diagnostics.message = "platform native texture guard and nozzle C API path are compile-enabled; this is not Unreal Engine runtime evidence";
    return diagnostics;
}

inline NozzleNativeDevice make_d3d11_native_device(d3d11_device_view device_view) noexcept {
    NozzleNativeDevice native_device{};
    native_device.backend = NOZZLE_BACKEND_D3D11;
    native_device.device = device_view.device;
    native_device.context = device_view.context;
    return native_device;
}

inline NozzleNativeDevice make_metal_native_device(metal_device_view device_view) noexcept {
    NozzleNativeDevice native_device{};
    native_device.backend = NOZZLE_BACKEND_METAL;
    native_device.device = device_view.device;
    native_device.context = nullptr;
    return native_device;
}

inline NozzleSenderDesc make_sender_desc(const char *sender_name, const char *application_name, std::uint32_t ring_buffer_size) noexcept {
    NozzleSenderDesc desc{};
    desc.name = sender_name;
    desc.application_name = application_name;
    desc.ring_buffer_size = ring_buffer_size;
    desc.allow_format_fallback = 0;
    desc.fallback_flags = NOZZLE_FALLBACK_NONE;
    desc.fallback_flags_valid = 1;
    return desc;
}

inline NozzleReceiverDesc make_receiver_desc(const char *sender_name, const char *application_name) noexcept {
    NozzleReceiverDesc desc{};
    desc.name = sender_name;
    desc.application_name = application_name;
    desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;
    return desc;
}

inline NozzleErrorCode create_d3d11_sender(
    const NozzleSenderDesc *sender_desc,
    d3d11_device_view device_view,
    bool selected_rhi_is_d3d11,
    NozzleSender **out_sender
) noexcept {
    if(out_sender == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }
    *out_sender = nullptr;

    const runtime_diagnostics diagnostics = make_runtime_diagnostics(selected_rhi_is_d3d11, false);
    if(!diagnostics.can_use_runtime) {
        return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    }

    if(sender_desc == nullptr || sender_desc->name == nullptr || device_view.device == nullptr || device_view.context == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    const NozzleNativeDevice native_device = make_d3d11_native_device(device_view);
    return nozzle_sender_create_with_native_device(sender_desc, &native_device, out_sender);
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

inline NozzleErrorCode create_metal_sender(
    const NozzleSenderDesc *sender_desc,
    metal_device_view device_view,
    bool selected_rhi_is_metal,
    NozzleSender **out_sender
) noexcept {
    if(out_sender == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }
    *out_sender = nullptr;

    const runtime_diagnostics diagnostics = make_runtime_diagnostics(false, selected_rhi_is_metal);
    if(!diagnostics.can_use_runtime) {
        return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
    }

    if(sender_desc == nullptr || sender_desc->name == nullptr || device_view.device == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    const NozzleNativeDevice native_device = make_metal_native_device(device_view);
    return nozzle_sender_create_with_native_device(sender_desc, &native_device, out_sender);
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

inline NozzleErrorCode create_receiver_for_backend(
    const NozzleReceiverDesc *receiver_desc,
    bool selected_rhi_is_d3d11,
    bool selected_rhi_is_metal,
    NozzleReceiver **out_receiver
) noexcept {
    if(out_receiver == nullptr) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }
    *out_receiver = nullptr;

    const runtime_diagnostics diagnostics = make_runtime_diagnostics(selected_rhi_is_d3d11, selected_rhi_is_metal);
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

inline NozzleErrorCode create_receiver(
    const NozzleReceiverDesc *receiver_desc,
    bool selected_rhi_is_d3d11,
    NozzleReceiver **out_receiver
) noexcept {
    return create_receiver_for_backend(receiver_desc, selected_rhi_is_d3d11, false, out_receiver);
}

inline NozzleErrorCode publish_native_texture(
    NozzleSender *sender,
    void *native_texture,
    std::uint32_t width,
    std::uint32_t height,
    NozzleTextureFormat storage_format,
    NozzleTextureFormat semantic_format
) noexcept {
    if(sender == nullptr || native_texture == nullptr || width == 0 || height == 0) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    return nozzle_sender_publish_native_texture_ex(
        sender,
        native_texture,
        width,
        height,
        storage_format,
        semantic_format
    );
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

inline NozzleErrorCode publish_d3d11_texture(NozzleSender *sender, d3d11_texture_view texture_view) noexcept {
    return publish_native_texture(
        sender,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format,
        texture_view.semantic_format
    );
}

inline NozzleErrorCode publish_metal_texture(NozzleSender *sender, metal_texture_view texture_view) noexcept {
    return publish_native_texture(
        sender,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format,
        texture_view.semantic_format
    );
}

inline NozzleErrorCode copy_frame_to_native_texture(
    NozzleFrame *frame,
    void *native_texture,
    std::uint32_t width,
    std::uint32_t height,
    NozzleTextureFormat format
) noexcept {
    if(frame == nullptr || native_texture == nullptr || width == 0 || height == 0) {
        return NOZZLE_ERROR_INVALID_ARGUMENT;
    }

#if NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE
    return nozzle_frame_copy_to_native_texture(
        frame,
        native_texture,
        width,
        height,
        format
    );
#else
    return NOZZLE_ERROR_UNSUPPORTED_BACKEND;
#endif
}

inline NozzleErrorCode copy_frame_to_d3d11_texture(NozzleFrame *frame, d3d11_texture_view texture_view) noexcept {
    return copy_frame_to_native_texture(
        frame,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format
    );
}

inline NozzleErrorCode copy_frame_to_metal_texture(NozzleFrame *frame, metal_texture_view texture_view) noexcept {
    return copy_frame_to_native_texture(
        frame,
        texture_view.native_texture,
        texture_view.width,
        texture_view.height,
        texture_view.storage_format
    );
}

} // namespace nozzle_unreal_native
