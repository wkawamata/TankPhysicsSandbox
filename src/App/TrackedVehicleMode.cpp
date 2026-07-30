#include "App/TrackedVehicleMode.h"
#include "App/CameraController.h"
#include "App/TankSettingsStore.h"
#include "App/TankVisualSettingsStore.h"
#include "Input/TankInputMapper.h"
#include "Physics/PhysicsEnvironmentSettingsJson.h"
#include "Physics/TankSettingsJson.h"
#include "Rendering/TankVisualSettingsJson.h"
#include "Rendering/TankModelExporter.h"
#include "Runtime/SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <ios>

namespace
{
    constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";
}

TrackedVehicleMode::TrackedVehicleMode()
{
}

float TrackedVehicleMode::NormalizeRawGamepadAxis(float value)
{
    const float normalized = std::clamp((value - 0.5f) * 2.0f, -1.0f, 1.0f);
    if (std::abs(normalized) <= kAnalogTrackDeadzone)
    {
        return 0.0f;
    }

    const float magnitude =
        (std::abs(normalized) - kAnalogTrackDeadzone) / (1.0f - kAnalogTrackDeadzone);
    return std::copysign(std::min(magnitude, 1.0f), normalized);
}

void TrackedVehicleMode::Enter(RtPbrSurvey::SceneRenderer& renderer)
{
    m_presenter.BuildScene(
        m_environmentSettings,
        m_visualSettings,
        m_settings);
    Engine::Scene& scene = m_presenter.GetScene();

    m_test.Initialize(m_settings, m_environmentSettings);
    m_appliedSettings = m_settings;
    m_appliedEnvironmentSettings = m_environmentSettings;
    m_paused = false;
    m_singleStep = false;

    UpdateSceneInternal(renderer);
    renderer.SetScene(scene);
    renderer.ReloadSceneResources(scene);
    renderer.SetDisplayInstanceCount(static_cast<int>(scene.instances.size()));

    m_active = true;
}

void TrackedVehicleMode::Exit()
{
    m_presenter.Clear();
    m_active = false;
}

void TrackedVehicleMode::UpdateSceneInternal(RtPbrSurvey::SceneRenderer&)
{
    m_presenter.UpdateScene(
        m_test.State(),
        m_settings,
        m_visualSettings,
        m_trackShoeDisplay,
        m_showTrackProxies,
        m_physicsDebugOverlay);
}

void TrackedVehicleMode::UpdateScene(RtPbrSurvey::SceneRenderer& renderer)
{
    UpdateSceneInternal(renderer);
    renderer.SetScene(m_presenter.GetScene());
}

void TrackedVehicleMode::UpdateInput(
    const Tank::Input::GamepadState& gamepadState,
    bool moveForward, bool moveBackward,
    bool turnLeft, bool turnRight, bool pivotTurnModifier,
    bool rollLeft, bool rollRight, bool brake)
{
    Tank::Physics::TankInput input;
    m_analogTracksConnected = gamepadState.connected && gamepadState.axisCount >= 4;
    const bool brakePressed = brake || gamepadState.brakePressed;

    m_analogLeftTrack =
        m_analogTracksConnected ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[3]) : 0.0f;
    m_analogRightTrack =
        m_analogTracksConnected ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[1]) : 0.0f;
    const float analogRollAxis0 =
        m_analogTracksConnected ? NormalizeRawGamepadAxis(gamepadState.rawAxes[0]) : 0.0f;
    const float analogRollAxis2 =
        m_analogTracksConnected ? NormalizeRawGamepadAxis(gamepadState.rawAxes[2]) : 0.0f;

    m_analogRoll = std::clamp((analogRollAxis0 + analogRollAxis2) * 0.5f, -1.0f, 1.0f);

    if (m_analogLeftTrack != 0.0f || m_analogRightTrack != 0.0f)
    {
        const float maximumMagnitude =
            std::max(std::abs(m_analogLeftTrack), std::abs(m_analogRightTrack));
        if (m_analogLeftTrack <= 0.0f && m_analogRightTrack <= 0.0f)
        {
            input.throttle = -maximumMagnitude;
            input.leftTrack = -m_analogLeftTrack / maximumMagnitude;
            input.rightTrack = -m_analogRightTrack / maximumMagnitude;
        }
        else
        {
            input.throttle = maximumMagnitude;
            input.leftTrack = m_analogLeftTrack / maximumMagnitude;
            input.rightTrack = m_analogRightTrack / maximumMagnitude;
        }
        input.roll = m_analogRoll != 0.0f
            ? m_analogRoll
            : (rollLeft ? 1.0f : (rollRight ? -1.0f : 0.0f));
        input.brake = brakePressed;
        m_test.SetInput(input);
        return;
    }

    input.throttle = moveForward ? 1.0f : (moveBackward ? -1.0f : 0.0f);
    input.roll = m_analogRoll != 0.0f
        ? m_analogRoll
        : (rollLeft ? 1.0f : (rollRight ? -1.0f : 0.0f));
    input.brake = brakePressed;

    if (turnLeft != turnRight)
    {
        if (input.throttle == 0.0f)
        {
            input.throttle = 1.0f;
            if (pivotTurnModifier)
            {
                input.leftTrack = turnLeft ? -1.0f : 1.0f;
                input.rightTrack = turnLeft ? 1.0f : -1.0f;
            }
            else
            {
                input.leftTrack = turnLeft ? 0.0f : 1.0f;
                input.rightTrack = turnLeft ? 1.0f : 0.0f;
            }
        }
        else
        {
            input.leftTrack = turnLeft ? 0.6f : 1.0f;
            input.rightTrack = turnLeft ? 1.0f : 0.6f;
        }
    }

    const bool gamepadActive =
        !m_analogTracksConnected &&
        gamepadState.connected &&
        (std::abs(gamepadState.leftStickX) > 0.05f ||
            std::abs(gamepadState.leftStickY) > 0.05f ||
            gamepadState.brakePressed);
    if (gamepadActive)
    {
        const bool keyboardBrake = input.brake;
        const float keyboardRoll = input.roll;
        input = Tank::Input::MapGamepadToTankInput(gamepadState);
        input.brake = input.brake || keyboardBrake;
        input.roll = keyboardRoll;
    }

    if (m_analogTracksConnected &&
        m_analogLeftTrack == 0.0f &&
        m_analogRightTrack == 0.0f &&
        input.throttle == 0.0f &&
        m_settings.neutralBrakeEnabled)
    {
        input.brakeAmount =
            std::clamp(m_settings.neutralBrakeAmount, 0.0f, 1.0f);
    }

    m_test.SetInput(input);
}

void TrackedVehicleMode::Step(
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    if (!m_paused || m_singleStep)
    {
        const Tank::Physics::TrackedVehicleTestState state = m_test.Step(kPhysicsFixedDt);
        UpdateSceneInternal(renderer);
        renderer.SetScene(m_presenter.GetScene());
        m_singleStep = false;
    }

    if (Engine::CameraState* camera = ActiveCamera())
    {
        if (!cameraController.IsDebugSlot())
        {
            cameraController.UpdateFollowCamera(
                m_test.State(),
                kPhysicsFixedDt,
                *camera);
        }
    }
}

void TrackedVehicleMode::Reset(
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    m_test.Initialize(
        m_settings,
        m_appliedEnvironmentSettings);
    m_appliedSettings = m_settings;
    m_singleStep = false;
    cameraController.ResetFollowState();
    UpdateSceneInternal(renderer);
    renderer.SetScene(m_presenter.GetScene());
}

void TrackedVehicleMode::ApplyMaterials(RtPbrSurvey::SceneRenderer& renderer)
{
    m_presenter.ApplyMaterials(m_visualSettings);
    renderer.ReloadSceneResources(m_presenter.GetScene());
}

Engine::CameraState* TrackedVehicleMode::ActiveCamera()
{
    return &m_presenter.GetScene().camera;
}

Engine::Scene& TrackedVehicleMode::GetScene()
{
    return m_presenter.GetScene();
}

bool TrackedVehicleMode::SaveTankSettings()
{
    Tank::App::TankSettingsStore store(m_tankSettingsSlot);
    return store.Write(m_settings, m_tankSettingsStatus);
}

bool TrackedVehicleMode::LoadTankSettings(
    bool apply,
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    Tank::App::TankSettingsStore store(m_tankSettingsSlot);
    Tank::Physics::TankSettings loaded = m_settings;
    if (!store.Read(loaded, m_tankSettingsStatus))
    {
        return false;
    }
    m_settings = loaded;
    if (apply)
    {
        Reset(renderer, cameraController);
    }
    return true;
}

bool TrackedVehicleMode::SaveTankVisualSettings()
{
    Tank::App::TankVisualSettingsStore store;
    return store.Write(m_visualSettings, m_tankVisualSettingsStatus);
}

bool TrackedVehicleMode::LoadTankVisualSettings(
    bool apply,
    RtPbrSurvey::SceneRenderer& renderer)
{
    Tank::App::TankVisualSettingsStore store;
    Tank::Rendering::TankVisualSettings loaded = m_visualSettings;
    if (!store.Read(loaded, m_tankVisualSettingsStatus))
    {
        return false;
    }
    m_visualSettings = loaded;
    if (apply)
    {
        ApplyMaterials(renderer);
    }
    return true;
}

bool TrackedVehicleMode::SaveEnvironmentSettings()
{
    const std::filesystem::path path(kEnvironmentSettingsPath);
    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    if (errorCode)
    {
        m_environmentSettingsStatus = "Save failed: " + errorCode.message();
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        m_environmentSettingsStatus = "Save failed: cannot open file";
        return false;
    }

    output << Tank::Physics::SerializePhysicsEnvironmentSettings(m_environmentSettings);
    if (!output)
    {
        m_environmentSettingsStatus = "Save failed: cannot write file";
        return false;
    }

    m_environmentSettingsStatus = std::string("Saved: ") + kEnvironmentSettingsPath;
    return true;
}

bool TrackedVehicleMode::LoadEnvironmentSettings()
{
    std::ifstream input(kEnvironmentSettingsPath, std::ios::binary);
    if (!input)
    {
        m_environmentSettingsStatus = "Load failed: no saved settings";
        return false;
    }

    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Tank::Physics::PhysicsEnvironmentSettings loaded = m_environmentSettings;
    std::string error;
    if (!Tank::Physics::DeserializePhysicsEnvironmentSettings(json, loaded, &error))
    {
        m_environmentSettingsStatus = "Load failed: " + error;
        return false;
    }

    m_environmentSettings = loaded;
    m_environmentSettingsStatus = std::string("Loaded: ") + kEnvironmentSettingsPath;
    return true;
}

bool TrackedVehicleMode::ExportTankModel()
{
    const TrackedVehicleScenePresenter::TrackedVehicleModel& model =
        m_presenter.Model();
    std::vector<Tank::Rendering::TankExportPart> parts = {
        { model.hullUpper, "Hull_Upper" },
        { model.hullLower, "Hull_Lower" },
        { model.upperStructureUpper, "UpperStructure_Upper" },
        { model.upperStructureLower, "UpperStructure_Lower" },
        { model.lowerStructureUpper, "LowerStructure_Upper" },
        { model.lowerStructureLower, "LowerStructure_Lower" },
        { model.forwardMarker, "ForwardMarker" },
    };
    for (int wheel = 0; wheel < m_test.State().wheelCount; ++wheel)
    {
        parts.push_back({
            model.wheels[static_cast<size_t>(wheel)],
            "Wheel_" + std::to_string(wheel) });
    }
    for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
    {
        for (int shoe = 0;
             shoe < TrackedVehicleScenePresenter::TrackedVehicleModel::
                 kTrackShoeCountPerTrack;
             ++shoe)
        {
            parts.push_back({
                model.trackShoes[static_cast<size_t>(track)]
                    [static_cast<size_t>(shoe)],
                "Track_" + std::to_string(track) +
                    "_Shoe_" + std::to_string(shoe) });
        }
    }
    return Tank::Rendering::ExportTankGltf(
        m_presenter.GetScene(),
        parts,
        m_test.State(),
        "Exports/Tank.gltf",
        m_tankModelExportStatus);
}
