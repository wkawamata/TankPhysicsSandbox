cmake_minimum_required(VERSION 3.25)

project(TankPhysicsSandbox LANGUAGES CXX)

include(CTest)

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

add_library(TankPhysicsCore STATIC
    src/Physics/BoxDropTest.cpp
    src/Physics/BoxDropTest.h
    src/Physics/PhysicsTypes.h
    src/Physics/PhysicsWorld.cpp
    src/Physics/PhysicsWorld.h
    src/Physics/TankController.cpp
    src/Physics/TankController.h
    src/Physics/TankTypes.h
)

target_include_directories(TankPhysicsCore
    PUBLIC
        src
)

target_link_libraries(TankPhysicsCore
    PUBLIC
        Jolt
)

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
        TankPhysicsCore
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

add_executable(TankPhysicsCli
    src/cli/TankPhysicsCli.cpp
)

target_link_libraries(TankPhysicsCli
    PRIVATE
        TankPhysicsCore
)

if(MSVC)
    target_compile_options(TankPhysicsCli PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
    add_executable(TankControllerTests
        tests/TankControllerTests.cpp
    )
    target_link_libraries(TankControllerTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TankControllerTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(PhysicsWorldTests
        tests/PhysicsWorldTests.cpp
    )
    target_link_libraries(PhysicsWorldTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(PhysicsWorldTests PRIVATE /W4 /permissive-)
    endif()

    add_test(
        NAME TankPhysics.BoxDrop
        COMMAND TankPhysicsCli --test box-drop --steps 300 --dt 0.0166667
    )
    set_tests_properties(TankPhysics.BoxDrop PROPERTIES
        PASS_REGULAR_EXPRESSION "PASS box-drop"
    )

    add_test(
        NAME TankPhysics.TankControllerBoundary
        COMMAND TankControllerTests
    )

    add_test(
        NAME TankPhysics.PhysicsWorld
        COMMAND PhysicsWorldTests
    )
endif()
