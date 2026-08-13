#pragma once

#include "Input/GamepadState.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/MapDefinition.h"
#include "Physics/MapDefinitionJson.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/TankVisualSettings.h"
#include "Rendering/TrackedVehicleScenePresenter.h"
#include "GltfLoader.h"

#include <filesystem>
#include <string>
#include <optional>
#include <windows.h>

namespace Engine { struct CameraState; class Scene; }
namespace RtPbrSurvey { class SceneRenderer; }
namespace Tank::App { class CameraController; }

class TrackedVehicleMode
{
public:
    TrackedVehicleMode();

    bool LoadTankModelAsset(const std::filesystem::path& path);

    void Enter(RtPbrSurvey::SceneRenderer& renderer);
    void Exit();

    void UpdateInput(
        const Tank::Input::GamepadState& gamepadState,
        bool moveForward, bool moveBackward,
        bool turnLeft, bool turnRight, bool pivotTurnModifier,
        bool rollLeft, bool rollRight, bool brake);

    void Step(
        RtPbrSurvey::SceneRenderer& renderer,
        Tank::App::CameraController& cameraController);

    void Reset(RtPbrSurvey::SceneRenderer& renderer, Tank::App::CameraController& cameraController);
    bool FireRecoil();
    void ApplyMaterials(RtPbrSurvey::SceneRenderer& renderer);
    void UpdateScene(RtPbrSurvey::SceneRenderer& renderer);
    void SelectMap(Tank::Physics::MapId mapId);
    void SelectCustomMap(const Tank::Physics::MapDocument& document);
    Tank::Physics::MapId SelectedMap() const { return m_selectedMap; }
    const std::string& ActiveMapName() const { return m_activeMapName; }

    Engine::CameraState* ActiveCamera();
    Engine::Scene& GetScene();

    Tank::Physics::TrackedVehicleTest& Test() { return m_test; }
    const Tank::Physics::TrackedVehicleTestState& TestState() const { return m_test.State(); }
    const Tank::Physics::TrackedDriverInput& DriverInput() const { return m_test.DriverInput(); }

    Tank::Physics::TankSettings& Settings() { return m_settings; }
    const Tank::Physics::TankSettings& AppliedSettings() const { return m_appliedSettings; }
    Tank::Physics::PhysicsEnvironmentSettings& EnvSettings() { return m_environmentSettings; }
    const Tank::Physics::PhysicsEnvironmentSettings& AppliedEnvSettings() const { return m_appliedEnvironmentSettings; }
    Tank::Rendering::TankVisualSettings& VisualSettings() { return m_visualSettings; }

    bool& Paused() { return m_paused; }
    bool& SingleStep() { return m_singleStep; }
    bool& PhysicsDebugOverlay() { return m_physicsDebugOverlay; }
    bool& TrackShoeDisplay() { return m_trackShoeDisplay; }
    bool& ShowTrackProxies() { return m_showTrackProxies; }
    bool& ShowDummyModel() { return m_showDummyModel; }
    bool& ShowGltfBody() { return m_showGltfBody; }
    bool& ShowGltfCannon() { return m_showGltfCannon; }
    bool& ShowGltfSide() { return m_showGltfSide; }
    const std::string& TankModelLoadStatus() const { return m_tankModelLoadStatus; }
    bool& TankVisualMaterialApplyPending() { return m_tankVisualMaterialApplyPending; }
    int& TankSettingsSlot() { return m_tankSettingsSlot; }
    bool& TankSettingsAutoLoad() { return m_tankSettingsAutoLoad; }
    bool& TankVisualSettingsAutoLoad() { return m_tankVisualSettingsAutoLoad; }
    std::string& TankSettingsStatus() { return m_tankSettingsStatus; }
    std::string& TankVisualSettingsStatus() { return m_tankVisualSettingsStatus; }
    std::string& EnvSettingsStatus() { return m_environmentSettingsStatus; }

    float AnalogLeftTrack() const { return m_analogLeftTrack; }
    float AnalogRightTrack() const { return m_analogRightTrack; }
    float AnalogRoll() const { return m_analogRoll; }
    bool AnalogTracksConnected() const { return m_analogTracksConnected; }
    bool AnalogTracksArmed() const { return m_analogTracksArmed; }

    bool SaveTankSettings();
    bool LoadTankSettings(bool apply, RtPbrSurvey::SceneRenderer& renderer, Tank::App::CameraController& cameraController);
    bool SaveTankVisualSettings();
    bool LoadTankVisualSettings(bool apply, RtPbrSurvey::SceneRenderer& renderer);
    bool SaveEnvironmentSettings();
    bool LoadEnvironmentSettings();
    bool ExportTankModel();
    std::string& TankModelExportPath() { return m_tankModelExportPath; }
    std::string& TankModelExportStatus() { return m_tankModelExportStatus; }
    bool& TankModelExportBinary() { return m_tankModelExportBinary; }

    bool IsActive() const { return m_active; }
    void SetPhysicsDebugOverlayDefault(bool enabled) { m_physicsDebugOverlay = enabled; }

    static constexpr float kPhysicsFixedDt = 1.0f / 60.0f;

private:
    static constexpr float kAnalogTrackDeadzone = 0.1f;

    float NormalizeRawGamepadAxis(float value);
    void UpdateSceneInternal(RtPbrSurvey::SceneRenderer& renderer);

    TrackedVehicleScenePresenter m_presenter;
    Tank::Physics::TrackedVehicleTest m_test;

    Tank::Physics::TankSettings m_settings;
    Tank::Physics::TankSettings m_appliedSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_environmentSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_appliedEnvironmentSettings;
    Tank::Physics::MapId m_selectedMap = Tank::Physics::MapId::ObstacleField;
    std::optional<Tank::Physics::MapDocument> m_customMap;
    Tank::Rendering::TankVisualSettings m_visualSettings;

    bool m_paused = false;
    bool m_singleStep = false;
    bool m_trackShoeDisplay = true;
    bool m_showTrackProxies = false;
    bool m_showDummyModel = true;
    bool m_showGltfBody = true;
    bool m_showGltfCannon = true;
    bool m_showGltfSide = true;
    bool m_physicsDebugOverlay = false;
    bool m_tankVisualMaterialApplyPending = false;
    bool m_active = false;

    int m_tankSettingsSlot = 0;
    bool m_tankSettingsAutoLoad = true;
    bool m_tankVisualSettingsAutoLoad = true;
    std::string m_tankSettingsStatus;
    std::string m_tankVisualSettingsStatus;
    std::string m_environmentSettingsStatus;
    std::string m_activeMapName = "Obstacle Field";
    std::string m_tankModelExportPath = "Exports/Tank.gltf";
    std::string m_tankModelExportStatus;
    bool m_tankModelExportBinary = false;
    Engine::GltfSceneAsset m_tankModelAsset;
    std::string m_tankModelLoadStatus;

    float m_analogLeftTrack = 0.0f;
    float m_analogRightTrack = 0.0f;
    float m_analogRoll = 0.0f;
    bool m_analogTracksConnected = false;
    bool m_analogTracksArmed = false;
};
