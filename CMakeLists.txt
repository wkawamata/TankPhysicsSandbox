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

find_package(nlohmann_json CONFIG REQUIRED)

add_library(TankPhysicsCore STATIC
    src/Physics/BoxDropTest.cpp
    src/Physics/BoxDropTest.h
    src/Physics/PhysicsTypes.h
    src/Physics/PhysicsWorld.cpp
    src/Physics/PhysicsWorld.h
    src/Physics/TankController.cpp
    src/Physics/TankController.h
    src/Physics/TankSettingsJson.cpp
    src/Physics/TankSettingsJson.h
    src/Physics/TankTypes.h
    src/Physics/TrackedVehicleTest.cpp
    src/Physics/TrackedVehicleTest.h
)

target_include_directories(TankPhysicsCore
    PUBLIC
        src
)

target_link_libraries(TankPhysicsCore
    PUBLIC
        Jolt
    PRIVATE
        nlohmann_json::nlohmann_json
)

# ── TankSandbox executable ─────────────────────────────────────
add_executable(TankSandbox
    src/main.cpp
    src/TankSandboxApp.cpp
    src/TankSandboxApp.h
    src/stdafx.h
)

target_precompile_headers(TankSandbox
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.h"
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

    add_executable(TankSettingsJsonTests
        tests/TankSettingsJsonTests.cpp
    )
    target_link_libraries(TankSettingsJsonTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TankSettingsJsonTests PRIVATE /W4 /permissive-)
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

    add_test(
        NAME TankPhysics.TankSettingsJson
        COMMAND TankSettingsJsonTests
    )

    add_executable(TrackedVehicleTests
        tests/TrackedVehicleTests.cpp
    )
    target_link_libraries(TrackedVehicleTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleForwardTests
        tests/TrackedVehicleForwardTests.cpp
    )
    target_link_libraries(TrackedVehicleForwardTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleForwardTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleSteeringTests
        tests/TrackedVehicleSteeringTests.cpp
    )
    target_link_libraries(TrackedVehicleSteeringTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleSteeringTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehiclePivotTests
        tests/TrackedVehiclePivotTests.cpp
    )
    target_link_libraries(TrackedVehiclePivotTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehiclePivotTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleStationaryTurnTests
        tests/TrackedVehicleStationaryTurnTests.cpp
    )
    target_link_libraries(TrackedVehicleStationaryTurnTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleStationaryTurnTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleInvertedTests
        tests/TrackedVehicleInvertedTests.cpp
    )
    target_link_libraries(TrackedVehicleInvertedTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleInvertedTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleRollTests
        tests/TrackedVehicleRollTests.cpp
    )
    target_link_libraries(TrackedVehicleRollTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleRollTests PRIVATE /W4 /permissive-)
    endif()

    add_executable(TrackedVehicleRideHeightTests
        tests/TrackedVehicleRideHeightTests.cpp
    )
    target_link_libraries(TrackedVehicleRideHeightTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TrackedVehicleRideHeightTests PRIVATE /W4 /permissive-)
    endif()

    add_test(
        NAME TankPhysics.TrackedVehicleNeutral
        COMMAND TrackedVehicleTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleForward
        COMMAND TrackedVehicleForwardTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleSteering
        COMMAND TrackedVehicleSteeringTests
    )

    add_test(
        NAME TankPhysics.TrackedVehiclePivot
        COMMAND TrackedVehiclePivotTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleStationaryTurn
        COMMAND TrackedVehicleStationaryTurnTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleInverted
        COMMAND TrackedVehicleInvertedTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleRoll
        COMMAND TrackedVehicleRollTests
    )

    add_test(
        NAME TankPhysics.TrackedVehicleRideHeight
        COMMAND TrackedVehicleRideHeightTests
    )
endif()
