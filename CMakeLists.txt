cmake_minimum_required(VERSION 3.25)

project(TankPhysicsSandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Jolt Physics ──────────────────────────────────────────────
set(DOUBLE_PRECISION OFF)
set(GENERATE_DEBUG_SYMBOLS ON)
set(INTERPROCEDURAL_OPTIMIZATION ON)
set(CPP_RTTI_ENABLED OFF)
set(CPP_EXCEPTIONS_ENABLED OFF)
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX ON)
set(USE_AVX2 ON)
set(USE_AVX512 OFF)
set(USE_LZCNT ON)
set(USE_TZCNT ON)
set(USE_F16C ON)
set(USE_FMADD ON)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
set(ENABLE_ALL_WARNINGS OFF)
set(OVERRIDE_CXX_FLAGS OFF)
add_subdirectory(External/JoltPhysics/Build External/JoltPhysics)

# ── RtPbrSurvey (D3D12 renderer) ──────────────────────────────
add_subdirectory(External/RtPbrSurvey)

find_package(directx-headers CONFIG REQUIRED)

# ── TankSandbox executable ─────────────────────────────────────
add_executable(TankSandbox
    Source/TankSandbox/Main.cpp
    src/TankSandboxApp.cpp
    src/TankSandboxApp.h
    src/stdafx.h
)

set_target_properties(TankSandbox PROPERTIES WIN32_EXECUTABLE TRUE)

target_include_directories(TankSandbox
    PRIVATE
        External/JoltPhysics
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_generated>
)

target_link_libraries(TankSandbox
    PRIVATE
        Jolt
        RtPbrSurvey
)

target_compile_definitions(TankSandbox
    PRIVATE
        WIN32 _WINDOWS _UNICODE UNICODE
)

if(MSVC)
    target_compile_options(TankSandbox PRIVATE /W4 /permissive-)
endif()

# ── D3D12 Agility SDK ─────────────────────────────────────────
set(D3D12_AGILITY_REDIST_DIR
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.direct3d.d3d12/1.618.3/build/native/bin/x64"
    CACHE PATH
    "Directory containing D3D12Core.dll and D3D12SDKLayers.dll for the D3D12 Agility SDK."
)

set(RTPBRSURVEY_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/External/RtPbrSurvey/Assets")
if(EXISTS "${RTPBRSURVEY_ASSET_DIR}")
    add_custom_command(TARGET TankSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${RTPBRSURVEY_ASSET_DIR}"
            "$<TARGET_FILE_DIR:TankSandbox>/Assets"
        COMMENT "Copying RtPbrSurvey runtime assets next to TankSandbox."
    )
endif()

if(EXISTS "${D3D12_AGILITY_REDIST_DIR}/D3D12Core.dll")
    add_custom_command(TARGET TankSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TankSandbox>/D3D12"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${D3D12_AGILITY_REDIST_DIR}/D3D12Core.dll"
            "$<TARGET_FILE_DIR:TankSandbox>/D3D12/D3D12Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${D3D12_AGILITY_REDIST_DIR}/D3D12SDKLayers.dll"
            "$<TARGET_FILE_DIR:TankSandbox>/D3D12/D3D12SDKLayers.dll"
        COMMENT "Copying D3D12 Agility SDK runtime next to TankSandbox."
    )
else()
    message(WARNING "D3D12 Agility SDK runtime not found at ${D3D12_AGILITY_REDIST_DIR}; hardware D3D12 device creation may fail with D3D12_ERROR_INVALID_REDIST.")
endif()
