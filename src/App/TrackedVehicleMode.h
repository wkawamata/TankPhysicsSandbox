#pragma once

#include "Input/GamepadState.h"
#include "Input/TankInputMapper.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/MapDefinition.h"
#include "Physics/MapDefinitionJson.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Map/MapManifest.h"
#include "Map/GltfHitMesh.h"
#include "Rendering/TankVisualSettings.h"
#include "Rendering/TrackedVehicleScenePresenter.h"
#include "Rendering/MortarRangeCue.h"
#include "GltfLoader.h"

#include <filesystem>
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <windows.h>
#include <Runtime/DebugLine.h>

namespace Engine { struct CameraState; class Scene; }
namespace RtPbrSurvey { class SceneRenderer; }
namespace Tank::App { class CameraController; }

class TrackedVehicleMode
{
public:
    TrackedVehicleMode();

    bool LoadTankModelAsset(const std::filesystem::path& path);

    bool Enter(RtPbrSurvey::SceneRenderer& renderer);
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
    bool SelectManifestMap(const std::filesystem::path& folder,
        const Tank::Map::Manifest& manifest, std::string& error);
    Tank::Physics::MapId SelectedMap() const { return m_selectedMap; }
    const std::string& ActiveMapName() const { return m_activeMapName; }
    const std::string& MapLoadStatus() const { return m_mapLoadStatus; }
    bool MapCleared() const { return m_mapCleared; }
    bool HasClearAreas() const { return m_manifestMap && !m_manifestMap->clearAreas.empty(); }
    const std::string& ClearedAreaName() const { return m_clearedAreaName; }

    Engine::CameraState* ActiveCamera();
    Engine::Scene& GetScene();

    Tank::Physics::TrackedVehicleTest& Test() { return m_test; }
    const Tank::Physics::TrackedVehicleTestState& TestState() const { return m_test.State(); }
    const Tank::Physics::TrackedDriverInput& DriverInput() const { return m_test.DriverInput(); }
    Tank::Rendering::MortarRangeCue MortarRangeCue() const;

    Tank::Physics::TankSettings& Settings() { return m_settings; }
    const Tank::Physics::TankSettings& AppliedSettings() const { return m_appliedSettings; }
    Tank::Physics::PhysicsEnvironmentSettings& EnvSettings() { return m_environmentSettings; }
    const Tank::Physics::PhysicsEnvironmentSettings& AppliedEnvSettings() const { return m_appliedEnvironmentSettings; }
    Tank::Rendering::TankVisualSettings& VisualSettings() { return m_visualSettings; }
    Tank::Input::TankInputMappingSettings& InputMappingSettings() { return m_inputMappingSettings; }
    const std::string& InputMappingStatus() const { return m_inputMappingStatus; }

    bool& Paused() { return m_paused; }
    bool& SingleStep() { return m_singleStep; }
    bool& RollingCheatWindowVisible() { return m_rollingCheatWindowVisible; }
    bool& RollingCheatWindowJapanese() { return m_rollingCheatWindowJapanese; }
    bool& PhysicsDebugOverlay() { return m_physicsDebugOverlay; }
    bool& MapHitMeshOverlay() { return m_mapHitMeshOverlay; }
    bool& MapMarkersVisible() { return m_mapMarkersVisible; }
    bool HasManifestMap() const { return m_manifestMap.has_value(); }
    bool& TrackShoeDisplay() { return m_visualSettings.showDummyTrackShoes; }
    bool& ShowTrackProxies() { return m_showTrackProxies; }
    bool& ShowDummyModel() { return m_visualSettings.showDummyBody; }
    bool& ShowDummyWheels() { return m_visualSettings.showDummyWheels; }
    bool& ShowGltfBody() { return m_showGltfBody; }
    bool& ShowGltfCannon() { return m_showGltfCannon; }
    bool& ShowGltfSide() { return m_showGltfSide; }
    const std::string& TankModelLoadStatus() const { return m_tankModelLoadStatus; }
    bool& TankVisualMaterialApplyPending() { return m_tankVisualMaterialApplyPending; }
    int& TankSettingsSlot() { return m_tankSettingsSlot; }
    int& RollingProfileSlot() { return m_rollingProfileSlot; }
    bool& TankSettingsAutoLoad() { return m_tankSettingsAutoLoad; }
    bool& TankVisualSettingsAutoLoad() { return m_tankVisualSettingsAutoLoad; }
    std::string& TankSettingsStatus() { return m_tankSettingsStatus; }
    std::string& RollingProfileStatus() { return m_rollingProfileStatus; }
    std::string& TankVisualSettingsStatus() { return m_tankVisualSettingsStatus; }
    std::string& EnvSettingsStatus() { return m_environmentSettingsStatus; }

    float AnalogLeftTrack() const { return m_analogLeftTrack; }
    float AnalogRightTrack() const { return m_analogRightTrack; }
    float AnalogRoll() const { return m_analogRoll; }
    bool AnalogTracksConnected() const { return m_analogTracksConnected; }
    bool AnalogTracksArmed() const { return m_analogTracksArmed; }

    bool SaveTankSettings();
    bool SaveRollingProfile();
    bool LoadTankSettings(bool apply, RtPbrSurvey::SceneRenderer& renderer, Tank::App::CameraController& cameraController);
    bool LoadRollingProfile(bool apply, RtPbrSurvey::SceneRenderer& renderer, Tank::App::CameraController& cameraController);
    bool SaveInputMappingSettings();
    bool LoadInputMappingSettings();
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
    float PhysicsStepTimeMs() const { return m_physicsStepTimeMs; }
    float PhysicsStepPeakTimeMs() const { return m_physicsStepPeakTimeMs; }
    float SceneUpdateTimeMs() const { return m_sceneUpdateTimeMs; }
    float SceneUpdatePeakTimeMs() const { return m_sceneUpdatePeakTimeMs; }
    void ResetFrameTimingPeaks();

    static constexpr float kPhysicsFixedDt = 1.0f / 60.0f;

private:
    static constexpr float kAnalogTrackDeadzone = 0.1f;

    float NormalizeRawGamepadAxis(float value);
    void UpdateSceneInternal(RtPbrSurvey::SceneRenderer& renderer);
    void UpdateClearCondition();

    TrackedVehicleScenePresenter m_presenter;
    Tank::Physics::TrackedVehicleTest m_test;

    Tank::Physics::TankSettings m_settings;
    Tank::Input::TankInputMappingSettings m_inputMappingSettings;
    std::string m_inputMappingStatus;
    Tank::Physics::TankSettings m_appliedSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_environmentSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_appliedEnvironmentSettings;
    Tank::Physics::MapId m_selectedMap = Tank::Physics::MapId::ObstacleField;
    std::optional<Tank::Physics::MapDocument> m_customMap;
    std::optional<Tank::Map::Manifest> m_manifestMap;
    std::filesystem::path m_manifestMapFolder;
    std::vector<Tank::Map::HitTriangleMesh> m_manifestHitMeshes;
    std::optional<size_t> m_mapHitMeshOverlayInstance;
    std::vector<size_t> m_mapMarkerInstances;
    std::vector<DirectX::XMFLOAT4X4> m_mapMarkerWorlds;
    std::vector<size_t> m_mapClearBeaconInstances;
    std::vector<DirectX::XMFLOAT4X4> m_mapClearBeaconWorlds;
    std::optional<size_t> m_clearedAreaIndex;
    uint32_t m_mapClearAreaMaterial = 0;
    uint32_t m_mapClearedAreaMaterial = 0;
    Tank::Rendering::TankVisualSettings m_visualSettings;

    bool m_paused = false;
    bool m_singleStep = false;
    bool m_rollingCheatWindowVisible = false;
    bool m_rollingCheatWindowJapanese = true;
    bool m_showTrackProxies = false;
    bool m_showGltfBody = true;
    bool m_showGltfCannon = true;
    bool m_showGltfSide = true;
    bool m_physicsDebugOverlay = false;
    bool m_mapHitMeshOverlay = false;
    bool m_mapMarkersVisible = true;
    bool m_tankVisualMaterialApplyPending = false;
    bool m_active = false;
    std::array<RtPbrSurvey::DebugLineHandle, 32> m_mortarRangeLines = {};
    bool m_mapCleared = false;

    int m_tankSettingsSlot = 0;
    int m_rollingProfileSlot = 0;
    bool m_tankSettingsAutoLoad = true;
    bool m_tankVisualSettingsAutoLoad = true;
    std::string m_tankSettingsStatus;
    std::string m_rollingProfileStatus;
    std::string m_tankVisualSettingsStatus;
    std::string m_environmentSettingsStatus;
    std::string m_activeMapName = "Obstacle Field";
    std::string m_mapLoadStatus;
    std::string m_clearedAreaName;
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
    float m_physicsStepTimeMs = 0.0f;
    float m_physicsStepPeakTimeMs = 0.0f;
    float m_sceneUpdateTimeMs = 0.0f;
    float m_sceneUpdatePeakTimeMs = 0.0f;
    std::uint64_t m_loggedRollingTraceSequence = 0;
};
