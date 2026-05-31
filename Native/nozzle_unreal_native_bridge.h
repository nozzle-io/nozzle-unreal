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

const char *supported_backend_name() noexcept;
const char *unsupported_runtime_message() noexcept;
runtime_diagnostics make_runtime_diagnostics(bool selected_rhi_is_d3d11, bool selected_rhi_is_metal) noexcept;
NozzleNativeDevice make_d3d11_native_device(d3d11_device_view device_view) noexcept;
NozzleNativeDevice make_metal_native_device(metal_device_view device_view) noexcept;
NozzleSenderDesc make_sender_desc(const char *sender_name, const char *application_name, std::uint32_t ring_buffer_size) noexcept;
NozzleReceiverDesc make_receiver_desc(const char *sender_name, const char *application_name) noexcept;
NozzleErrorCode create_d3d11_sender(const NozzleSenderDesc *sender_desc, d3d11_device_view device_view, bool selected_rhi_is_d3d11, NozzleSender **out_sender) noexcept;
NozzleErrorCode create_metal_sender(const NozzleSenderDesc *sender_desc, metal_device_view device_view, bool selected_rhi_is_metal, NozzleSender **out_sender) noexcept;
NozzleErrorCode create_receiver(const NozzleReceiverDesc *receiver_desc, bool selected_rhi_is_d3d11, NozzleReceiver **out_receiver) noexcept;
NozzleErrorCode create_receiver_for_backend(const NozzleReceiverDesc *receiver_desc, bool selected_rhi_is_d3d11, bool selected_rhi_is_metal, NozzleReceiver **out_receiver) noexcept;
NozzleErrorCode publish_d3d11_texture(NozzleSender *sender, d3d11_texture_view texture_view) noexcept;
NozzleErrorCode publish_metal_texture(NozzleSender *sender, metal_texture_view texture_view) noexcept;
NozzleErrorCode copy_frame_to_d3d11_texture(NozzleFrame *frame, d3d11_texture_view texture_view) noexcept;
NozzleErrorCode copy_frame_to_metal_texture(NozzleFrame *frame, metal_texture_view texture_view) noexcept;

} // namespace nozzle_unreal_native
