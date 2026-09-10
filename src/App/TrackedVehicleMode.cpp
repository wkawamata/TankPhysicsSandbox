#include "App/TrackedVehicleMode.h"
#include "App/CameraController.h"
#include "App/TankSettingsStore.h"
#include "App/TankVisualSettingsStore.h"
#include "Input/TankInputMapper.h"
#include "Map/MapClearCondition.h"
#include "Physics/PhysicsEnvironmentSettingsJson.h"
#include "Physics/TankSettingsJson.h"
#include "Rendering/TankVisualSettingsJson.h"
#include "Rendering/TankModelExporter.h"
#include "Rendering/MapVisualLoader.h"
#include "Runtime/SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <filesystem>
#include <fstream>
#include <ios>

namespace
{
    constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";

    Tank::Physics::MapSpawn ManifestSpawn(const Tank::Map::Manifest& manifest)
    {
        constexpr float degreesToRadians = 3.14159265358979323846f / 180.0f;
        Tank::Physics::MapSpawn spawn;
        spawn.position = { manifest.playerSpawn.position[0], manifest.playerSpawn.position[1],
            manifest.playerSpawn.position[2] };
        spawn.yawRadians = manifest.playerSpawn.rotationDegrees[1] * degreesToRadians;
        return spawn;
    }
}

TrackedVehicleMode::TrackedVehicleMode()
{
}

bool TrackedVehicleMode::LoadTankModelAsset(const std::filesystem::path& path)
{
    Engine::GltfSceneAssetLoadResult result =
        Engine::LoadGltfSceneAsset(path.string());
    if (!result)
    {
        m_tankModelAsset = {};
        m_tankModelLoadStatus = "Load failed: " + result.message;
        return false;
    }

    m_tankModelAsset = std::move(result.asset);
    m_tankModelLoadStatus = "Loaded: " + path.filename().string();
    return true;
}

void TrackedVehicleMode::SelectMap(Tank::Physics::MapId mapId)
{
    m_customMap.reset();
    m_manifestMap.reset();
    m_manifestHitMeshes.clear();
    m_mapHitMeshOverlayInstance.reset();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_selectedMap = mapId;
    const Tank::Physics::MapDefinition& definition =
        Tank::Physics::GetMapDefinition(mapId);
    m_environmentSettings = definition.environment;
    m_activeMapName = definition.name;
}

void TrackedVehicleMode::SelectCustomMap(
    const Tank::Physics::MapDocument& document)
{
    m_customMap = document;
    m_manifestMap.reset();
    m_manifestHitMeshes.clear();
    m_mapHitMeshOverlayInstance.reset();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_environmentSettings = document.environment;
    m_activeMapName = document.name;
}

bool TrackedVehicleMode::SelectManifestMap(const std::filesystem::path& folder,
    const Tank::Map::Manifest& manifest, std::string& error)
{
    std::vector<Tank::Map::HitTriangleMesh> meshes;
    meshes.reserve(manifest.instances.size());
    for (const Tank::Map::Instance& instance : manifest.instances)
    {
        const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
        Tank::Map::HitTriangleMesh local;
        if (!Tank::Map::LoadGltfHitMesh(folder / std::filesystem::path(assetUtf8), local, error))
        {
            error = "Cannot load HitMesh '" + instance.asset + "': " + error;
            return false;
        }
        Tank::Map::HitTriangleMesh world;
        if (!Tank::Map::TransformHitMesh(local, instance.transform, world, error))
        {
            error = "Cannot place HitMesh '" + instance.asset + "': " + error;
            return false;
        }
        meshes.push_back(std::move(world));
    }
    if (meshes.empty())
    {
        error = "Manifest contains no model instances.";
        return false;
    }
    m_customMap.reset();
    m_manifestMap = manifest;
    m_manifestMapFolder = folder;
    m_manifestHitMeshes = std::move(meshes);
    m_mapHitMeshOverlayInstance.reset();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_environmentSettings = {};
    m_activeMapName = folder.filename().string();
    m_mapLoadStatus = "Loaded Manifest map: " + m_activeMapName;
    error.clear();
    return true;
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

bool TrackedVehicleMode::Enter(RtPbrSurvey::SceneRenderer& renderer)
{
    m_mapHitMeshOverlayInstance.reset();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    const std::vector<Tank::Physics::MapPrimitive> mapPrimitives = m_customMap ?
        m_customMap->primitives :
        Tank::Physics::BuildMapPrimitives(m_selectedMap, m_environmentSettings);
    m_presenter.BuildScene(
        m_environmentSettings,
        mapPrimitives,
        m_visualSettings,
        m_settings,
        m_tankModelAsset.IsValid() ? &m_tankModelAsset : nullptr,
        !m_manifestMap.has_value());
    Engine::Scene& scene = m_presenter.GetScene();

    if (m_manifestMap)
    {
        const uint32_t mapMaterial = m_presenter.SceneBuilder().AddSolidColorMaterial(210, 210, 210, 255);
        const uint32_t hitMeshMaterial =
            m_presenter.SceneBuilder().AddSolidColorMaterial(255, 45, 190, 255);
        const uint32_t spawnMaterial =
            m_presenter.SceneBuilder().AddSolidColorMaterial(255, 190, 35, 255);
        const uint32_t clearAreaMaterial =
            m_presenter.SceneBuilder().AddSolidColorMaterial(35, 225, 235, 255);
        const uint32_t clearedAreaMaterial =
            m_presenter.SceneBuilder().AddSolidColorMaterial(70, 255, 90, 255);
        const Engine::SceneMeshId markerCube = m_presenter.SceneBuilder().AddCube(1.0f);
        std::string error;
        size_t hitMeshOverlayInstance = 0;
        if (!Tank::Rendering::AppendMapVisuals(m_presenter.SceneBuilder(), m_manifestMapFolder,
            *m_manifestMap, mapMaterial, error) ||
            !Tank::Rendering::AppendMapHitMeshOverlay(m_presenter.SceneBuilder(),
                m_manifestHitMeshes, hitMeshMaterial, hitMeshOverlayInstance, error) ||
            !m_test.InitializeWithStaticMeshes(m_settings, m_environmentSettings,
                m_manifestHitMeshes, ManifestSpawn(*m_manifestMap), error))
        {
            m_mapLoadStatus = "Manifest map start failed: " + error;
            m_presenter.Clear();
            m_active = false;
            return false;
        }
        m_mapHitMeshOverlayInstance = hitMeshOverlayInstance;
        Tank::Rendering::AppendMapMarkers(m_presenter.SceneBuilder(), markerCube,
            *m_manifestMap, spawnMaterial, clearAreaMaterial, m_mapMarkerInstances);
        Tank::Rendering::AppendMapClearBeacons(m_presenter.SceneBuilder(), markerCube,
            *m_manifestMap, clearedAreaMaterial, m_mapClearBeaconInstances);
        m_mapClearAreaMaterial = clearAreaMaterial;
        m_mapClearedAreaMaterial = clearedAreaMaterial;
        m_mapMarkerWorlds.reserve(m_mapMarkerInstances.size());
        for (const size_t index : m_mapMarkerInstances)
            m_mapMarkerWorlds.push_back(scene.instances[index].world);
        m_mapClearBeaconWorlds.reserve(m_mapClearBeaconInstances.size());
        for (const size_t index : m_mapClearBeaconInstances)
            m_mapClearBeaconWorlds.push_back(scene.instances[index].world);
    }
    else
    {
        m_test.Initialize(
            m_settings,
            m_environmentSettings,
            mapPrimitives,
            m_customMap ? m_customMap->spawn : Tank::Physics::MapSpawn {});
    }
    m_appliedSettings = m_settings;
    m_appliedEnvironmentSettings = m_environmentSettings;
    m_paused = false;
    m_singleStep = false;
    m_analogTracksArmed = false;
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_clearedAreaIndex.reset();
    UpdateClearCondition();

    UpdateSceneInternal(renderer);
    renderer.SetScene(scene);
    renderer.ReloadSceneResources(scene);
    renderer.SetDisplayInstanceCount(static_cast<int>(scene.instances.size()));

    m_active = true;
    return true;
}

void TrackedVehicleMode::Exit()
{
    m_presenter.Clear();
    m_mapHitMeshOverlayInstance.reset();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_active = false;
}

void TrackedVehicleMode::UpdateSceneInternal(RtPbrSurvey::SceneRenderer&)
{
    m_presenter.UpdateScene(
        m_test.State(),
        m_settings,
        m_visualSettings,
        m_visualSettings.showDummyTrackShoes,
        m_showTrackProxies,
        m_physicsDebugOverlay,
        m_visualSettings.showDummyBody,
        m_visualSettings.showDummyWheels,
        m_showGltfBody,
        m_showGltfCannon,
        m_showGltfSide);
    if (m_mapHitMeshOverlayInstance &&
        *m_mapHitMeshOverlayInstance < m_presenter.GetScene().instances.size())
    {
        Engine::InstanceData& instance =
            m_presenter.GetScene().instances[*m_mapHitMeshOverlayInstance];
        instance.prevWorld = instance.world;
        DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(
            m_mapHitMeshOverlay
                ? DirectX::XMMatrixIdentity()
                : DirectX::XMMatrixScaling(0.0f, 0.0f, 0.0f)));
    }
    for (size_t marker = 0; marker < m_mapMarkerInstances.size(); ++marker)
    {
        const size_t instanceIndex = m_mapMarkerInstances[marker];
        if (instanceIndex >= m_presenter.GetScene().instances.size()) continue;
        Engine::InstanceData& instance = m_presenter.GetScene().instances[instanceIndex];
        instance.prevWorld = instance.world;
        if (marker >= Tank::Rendering::kPlayerStartMarkerPartCount)
        {
            const size_t clearAreaIndex =
                (marker - Tank::Rendering::kPlayerStartMarkerPartCount) /
                Tank::Rendering::kClearAreaMarkerPartCount;
            instance.materialId = m_clearedAreaIndex == clearAreaIndex
                ? m_mapClearedAreaMaterial
                : m_mapClearAreaMaterial;
        }
        if (m_mapMarkersVisible)
        {
            instance.world = m_mapMarkerWorlds[marker];
        }
        else
        {
            DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(
                DirectX::XMMatrixScaling(0.0f, 0.0f, 0.0f)));
        }
    }
    for (size_t beacon = 0; beacon < m_mapClearBeaconInstances.size(); ++beacon)
    {
        const size_t instanceIndex = m_mapClearBeaconInstances[beacon];
        if (instanceIndex >= m_presenter.GetScene().instances.size()) continue;
        Engine::InstanceData& instance = m_presenter.GetScene().instances[instanceIndex];
        instance.prevWorld = instance.world;
        if (m_clearedAreaIndex == beacon)
        {
            instance.world = m_mapClearBeaconWorlds[beacon];
        }
        else
        {
            DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(
                DirectX::XMMatrixScaling(0.0f, 0.0f, 0.0f)));
        }
    }
}

void TrackedVehicleMode::UpdateClearCondition()
{
    if (m_mapCleared || !m_manifestMap) return;
    const Tank::Physics::Vec3& position = m_test.State().bodyPosition;
    const Tank::Map::ClearArea* area = Tank::Map::FindContainingClearArea(
        *m_manifestMap, { position.x, position.y, position.z });
    if (area == nullptr) return;
    m_mapCleared = true;
    const Tank::Map::ClearArea* firstArea = m_manifestMap->clearAreas.data();
    m_clearedAreaIndex = static_cast<size_t>(area - firstArea);
    m_clearedAreaName = area->name.empty() ? area->id : area->name;
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
    if (!m_analogTracksConnected)
    {
        m_analogTracksArmed = false;
    }
    else if (!m_analogTracksArmed)
    {
        const bool tracksNeutral =
            std::abs(gamepadState.rawAxes[1] - 0.5f) <= 0.05f &&
            std::abs(gamepadState.rawAxes[3] - 0.5f) <= 0.05f;
        m_analogTracksArmed = tracksNeutral;
    }
    const bool useAnalogTracks = m_analogTracksConnected && m_analogTracksArmed;
    const bool brakePressed = brake || gamepadState.brakePressed;

    m_analogLeftTrack =
        useAnalogTracks ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[3]) : 0.0f;
    m_analogRightTrack =
        useAnalogTracks ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[1]) : 0.0f;
    const float analogRollAxis0 =
        useAnalogTracks ? NormalizeRawGamepadAxis(gamepadState.rawAxes[0]) : 0.0f;
    const float analogRollAxis2 =
        useAnalogTracks ? NormalizeRawGamepadAxis(gamepadState.rawAxes[2]) : 0.0f;

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
        m_test.Step(kPhysicsFixedDt);
        UpdateClearCondition();
        UpdateSceneInternal(renderer);
        renderer.SetScene(m_presenter.GetScene());
        m_singleStep = false;
    }

    if (Engine::CameraState* camera = ActiveCamera())
    {
        cameraController.UpdateFollowCamera(
            m_test.State(),
            kPhysicsFixedDt,
            *camera);
    }
}

void TrackedVehicleMode::Reset(
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    const Tank::Physics::TrackedVehicleTestState previousState = m_test.State();
    Engine::CameraState* camera = ActiveCamera();
    const std::vector<Tank::Physics::MapPrimitive> mapPrimitives = m_customMap ?
        m_customMap->primitives :
        Tank::Physics::BuildMapPrimitives(
            m_selectedMap,
            m_environmentSettings);
    if (m_manifestMap)
    {
        std::string error;
        if (!m_test.InitializeWithStaticMeshes(m_settings, m_environmentSettings,
            m_manifestHitMeshes, ManifestSpawn(*m_manifestMap), error))
            m_mapLoadStatus = "Manifest map reset failed: " + error;
    }
    else
    {
        m_test.Initialize(
            m_settings, m_environmentSettings, mapPrimitives,
            m_customMap ? m_customMap->spawn : Tank::Physics::MapSpawn {});
    }
    m_appliedSettings = m_settings;
    m_appliedEnvironmentSettings = m_environmentSettings;
    m_singleStep = false;
    m_analogTracksArmed = false;
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_clearedAreaIndex.reset();
    UpdateClearCondition();
    if (camera != nullptr)
    {
        cameraController.OnTankTeleported(previousState, m_test.State(), *camera);
    }
    else
    {
        cameraController.ResetFollowState();
    }
    UpdateSceneInternal(renderer);
    renderer.SetScene(m_presenter.GetScene());
}

bool TrackedVehicleMode::FireRecoil()
{
    const bool applied = m_test.ApplyConfiguredRecoil();
    if (applied && m_paused)
    {
        m_singleStep = true;
    }
    return applied;
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
        UpdateScene(renderer);
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
    std::filesystem::path exportPath(m_tankModelExportPath);
    exportPath.replace_extension(m_tankModelExportBinary ? ".glb" : ".gltf");
    exportPath = std::filesystem::absolute(exportPath).lexically_normal();
    return Tank::Rendering::ExportTankGltf(
        m_presenter.GetScene(),
        parts,
        m_test.State(),
        exportPath,
        m_tankModelExportBinary,
        m_tankModelExportStatus);
}
