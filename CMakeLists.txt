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

# ── TankSandbox executable ─────────────────────────────────────
add_executable(TankSandbox
    src/main.cpp
    src/TankSandboxApp.cpp
    src/TankSandboxApp.h
    src/stdafx.h
)

set_target_properties(TankSandbox PROPERTIES WIN32_EXECUTABLE TRUE)

target_include_directories(TankSandbox
    PRIVATE
        External/JoltPhysics
)

target_link_libraries(TankSandbox
    PRIVATE
        Jolt
        RtPbrSurvey::SceneRenderer
)

target_compile_definitions(TankSandbox
    PRIVATE
        WIN32 _WINDOWS _UNICODE UNICODE
)

if(MSVC)
    target_compile_options(TankSandbox PRIVATE /W4 /permissive)
endif()

# ── D3D12 Agility SDK ─────────────────────────────────────────
rtpbrsurvey_copy_runtime_files(TankSandbox)
