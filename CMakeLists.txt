cmake_minimum_required(VERSION 3.25)

project(TankPhysicsSandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Jolt Physics ──────────────────────────────────────────────
# Options must be set before add_subdirectory as Jolt reads them at configure time.
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

# Match project's dynamic CRT (/MDd /MD) instead of Jolt's default static CRT
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)

# Suppress Jolt's own warning flags so they don't leak into our target
set(ENABLE_ALL_WARNINGS OFF)
set(OVERRIDE_CXX_FLAGS OFF)

add_subdirectory(External/JoltPhysics/Build External/JoltPhysics)

# ── TankSandbox executable ─────────────────────────────────────
add_executable(TankSandbox
    Source/TankSandbox/Main.cpp
)

target_compile_features(TankSandbox PRIVATE cxx_std_20)
target_include_directories(TankSandbox PRIVATE External/JoltPhysics)
target_link_libraries(TankSandbox PRIVATE Jolt)

if(MSVC)
    target_compile_options(TankSandbox PRIVATE /W4 /permissive-)
endif()
