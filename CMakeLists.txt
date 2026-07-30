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
    src/Physics/PhysicsEnvironmentSettings.h
    src/Physics/PhysicsEnvironmentSettingsJson.cpp
    src/Physics/PhysicsEnvironmentSettingsJson.h
    src/Physics/PhysicsWorld.cpp
    src/Physics/PhysicsWorld.h
    src/Physics/TankController.cpp
    src/Physics/TankController.h
    src/Physics/TankSettingsJson.cpp
    src/Physics/TankSettingsJson.h
    src/Physics/TankTypes.h
    src/Physics/TestObstacleLayout.cpp
    src/Physics/TestObstacleLayout.h
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

add_library(TankInputCore STATIC
    src/Input/GamepadState.h
    src/Input/TankInputMapper.cpp
    src/Input/TankInputMapper.h
)

target_include_directories(TankInputCore
    PUBLIC
        src
)

set(TANK_GAMEINPUT_VERSION "3.5.262" CACHE STRING "Microsoft.GameInput NuGet version.")
set(TANK_GAMEINPUT_PACKAGE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/packages/Microsoft.GameInput.${TANK_GAMEINPUT_VERSION}"
    CACHE PATH
    "Microsoft.GameInput NuGet package directory.")
set(TANK_GAMEINPUT_INCLUDE_DIR "${TANK_GAMEINPUT_PACKAGE_DIR}/native/include/v0")
set(TANK_GAMEINPUT_LIBRARY "${TANK_GAMEINPUT_PACKAGE_DIR}/native/lib/x64/GameInput.lib")

if(NOT EXISTS "${TANK_GAMEINPUT_INCLUDE_DIR}/GameInput.h" OR
   NOT EXISTS "${TANK_GAMEINPUT_LIBRARY}")
    message(FATAL_ERROR
        "Microsoft.GameInput NuGet package was not found. "
        "Run scripts/Restore-GameInput.ps1.")
endif()

add_library(TankWindowsInput STATIC
    src/Platform/Windows/WindowsGamepad.cpp
    src/Platform/Windows/WindowsGamepad.h
)

target_include_directories(TankWindowsInput
    PUBLIC
        src
    PRIVATE
        "${TANK_GAMEINPUT_INCLUDE_DIR}"
)

target_compile_definitions(TankWindowsInput PRIVATE USING_GAMEINPUT)

target_link_libraries(TankWindowsInput
    PUBLIC
        "${TANK_GAMEINPUT_LIBRARY}"
)

# ── TankSandbox executable ─────────────────────────────────────
add_executable(TankSandbox
    src/main.cpp
    src/App/CameraController.cpp
    src/App/CameraController.h
    src/App/CameraSettingsStore.cpp
    src/App/CameraSettingsStore.h
    src/App/TankSettingsStore.cpp
    src/App/TankSettingsStore.h
    src/App/TankVisualSettingsStore.cpp
    src/App/TankVisualSettingsStore.h
    src/Rendering/BoxDropScenePresenter.h
    src/Rendering/BoxDropScenePresenter.cpp
    src/Rendering/CameraSettings.h
    src/Rendering/CameraSettingsJson.cpp
    src/Rendering/TankVisualSettings.h
    src/Rendering/TankVisualSettingsJson.cpp
    src/Rendering/TankVisualSettingsJson.h
    src/Rendering/PhysicsDebugOverlay.h
    src/Rendering/PhysicsDebugOverlay.cpp
    src/Rendering/TrackedVehicleScenePresenter.h
    src/Rendering/TrackedVehicleScenePresenter.cpp
    src/Ui/CameraPanel.cpp
    src/Ui/CameraPanel.h
    src/Ui/RendererSettingsPanel.cpp
    src/Ui/RendererSettingsPanel.h
    src/Ui/TrackedVehiclePanel.cpp
    src/Ui/TrackedVehiclePanel.h
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
        TankInputCore
        TankWindowsInput
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
    add_executable(CameraSettingsStoreTests
        src/App/CameraSettingsStore.cpp
        src/Rendering/CameraSettingsJson.cpp
        tests/CameraSettingsStoreTests.cpp
    )
    target_include_directories(CameraSettingsStoreTests PRIVATE src)
    target_link_libraries(CameraSettingsStoreTests PRIVATE nlohmann_json::nlohmann_json)

    add_executable(TankSettingsStoreTests
        src/App/TankSettingsStore.cpp
        tests/TankSettingsStoreTests.cpp
    )
    target_include_directories(TankSettingsStoreTests PRIVATE src)
    target_link_libraries(TankSettingsStoreTests PRIVATE TankPhysicsCore nlohmann_json::nlohmann_json)

    add_executable(TankVisualSettingsStoreTests
        src/App/TankVisualSettingsStore.cpp
        src/Rendering/TankVisualSettingsJson.cpp
        tests/TankVisualSettingsStoreTests.cpp
    )
    target_include_directories(TankVisualSettingsStoreTests PRIVATE src)
    target_link_libraries(TankVisualSettingsStoreTests PRIVATE nlohmann_json::nlohmann_json)

    add_test(
        NAME TankApp.CameraSettingsStore
        COMMAND CameraSettingsStoreTests
    )
    add_test(
        NAME TankApp.TankSettingsStore
        COMMAND TankSettingsStoreTests
    )
    add_test(
        NAME TankApp.TankVisualSettingsStore
        COMMAND TankVisualSettingsStoreTests
    )

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

    add_executable(PhysicsEnvironmentSettingsJsonTests
        tests/PhysicsEnvironmentSettingsJsonTests.cpp
    )
    target_link_libraries(PhysicsEnvironmentSettingsJsonTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(PhysicsEnvironmentSettingsJsonTests PRIVATE /W4 /permissive-)
    endif()

    add_test(
        NAME TankPhysics.PhysicsEnvironmentSettingsJson
        COMMAND PhysicsEnvironmentSettingsJsonTests
    )

    add_executable(TankVisualSettingsJsonTests
        src/Rendering/TankVisualSettingsJson.cpp
        tests/TankVisualSettingsJsonTests.cpp
    )
    target_include_directories(TankVisualSettingsJsonTests PRIVATE src)
    target_link_libraries(TankVisualSettingsJsonTests PRIVATE nlohmann_json::nlohmann_json)
    if(MSVC)
        target_compile_options(TankVisualSettingsJsonTests PRIVATE /W4 /permissive-)
    endif()
    add_test(
        NAME TankRendering.TankVisualSettingsJson
        COMMAND TankVisualSettingsJsonTests
    )

    add_executable(CameraSettingsJsonTests
        src/Rendering/CameraSettingsJson.cpp
        tests/CameraSettingsJsonTests.cpp
    )
    target_include_directories(CameraSettingsJsonTests PRIVATE src)
    target_link_libraries(CameraSettingsJsonTests PRIVATE nlohmann_json::nlohmann_json)
    if(MSVC)
        target_compile_options(CameraSettingsJsonTests PRIVATE /W4 /permissive-)
    endif()
    add_test(
        NAME TankRendering.CameraSettingsJson
        COMMAND CameraSettingsJsonTests
    )

    add_executable(TestObstacleLayoutTests
        tests/TestObstacleLayoutTests.cpp
    )
    target_link_libraries(TestObstacleLayoutTests
        PRIVATE
            TankPhysicsCore
    )
    if(MSVC)
        target_compile_options(TestObstacleLayoutTests PRIVATE /W4 /permissive-)
    endif()

    add_test(
        NAME TankPhysics.TestObstacleLayout
        COMMAND TestObstacleLayoutTests
    )

    add_executable(TankInputMapperTests
        tests/TankInputMapperTests.cpp
    )
    target_link_libraries(TankInputMapperTests
        PRIVATE
            TankInputCore
    )
    if(MSVC)
        target_compile_options(TankInputMapperTests PRIVATE /W4 /permissive-)
    endif()

    add_test(
        NAME TankInput.Mapper
        COMMAND TankInputMapperTests
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
