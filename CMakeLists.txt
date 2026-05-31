cmake_minimum_required(VERSION 3.20)

project(nozzle_unreal_native_bridge LANGUAGES CXX)

option(NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE "Compile the native bridge paths that call the nozzle C API. This checks symbols at compile time but does not link a nozzle binary." ON)

add_library(nozzle_unreal_native_bridge OBJECT
    Native/nozzle_unreal_native_bridge.cpp
)
add_library(nozzle_unreal_native_bridge_compile_check OBJECT
    Native/nozzle_unreal_native_bridge_compile_check.cpp
)

foreach(target nozzle_unreal_native_bridge nozzle_unreal_native_bridge_compile_check)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Native
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/nozzle/include
    )
    target_compile_definitions(${target} PRIVATE
        NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE=$<BOOL:${NOZZLE_UNREAL_NATIVE_WITH_NOZZLE_CORE}>
    )

    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            NOZZLE_UNREAL_NATIVE_TARGET_WIN64=1
            NOZZLE_UNREAL_NATIVE_TARGET_MACOS=0
            NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME=1
            NOZZLE_UNREAL_NATIVE_METAL_RUNTIME=0
        )
    elseif(APPLE)
        target_compile_definitions(${target} PRIVATE
            NOZZLE_UNREAL_NATIVE_TARGET_WIN64=0
            NOZZLE_UNREAL_NATIVE_TARGET_MACOS=1
            NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME=0
            NOZZLE_UNREAL_NATIVE_METAL_RUNTIME=1
        )
    else()
        target_compile_definitions(${target} PRIVATE
            NOZZLE_UNREAL_NATIVE_TARGET_WIN64=0
            NOZZLE_UNREAL_NATIVE_TARGET_MACOS=0
            NOZZLE_UNREAL_NATIVE_D3D11_RUNTIME=0
            NOZZLE_UNREAL_NATIVE_METAL_RUNTIME=0
        )
    endif()
endforeach()
