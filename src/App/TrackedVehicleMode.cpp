#include "App/TrackedVehicleMode.h"
#include "Diagnostics/Log.h"
#include "Input/TankInputMappingJson.h"
#include "Input/InputDeviceProfilesJson.h"
#include <fstream>
#include "App/CameraController.h"
#include "App/RollingProfileStore.h"
#include "App/MortarProfileStore.h"
#include "App/TankSettingsStore.h"
#include "App/TankVisualSettingsStore.h"
#include "Input/TankInputMapper.h"
#include "Map/GltfRoles.h"
#include "Map/MapClearCondition.h"
#include "Physics/PhysicsEnvironmentSettingsJson.h"
#include "Physics/TankSettingsJson.h"
#include "Rendering/TankVisualSettingsJson.h"
#include "Rendering/TankModelExporter.h"
#include "Rendering/MortarRangeGeometry.h"
#include "Rendering/MapVisualLoader.h"
#include "Runtime/SceneRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <DirectXMath.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>

namespace
{
    constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";
    constexpr const char* kTankSettingsDirectory = TANK_SOURCE_CONFIG_DIR;
    constexpr const char* kRollingProfileDirectory = TANK_SOURCE_CONFIG_DIR;
    constexpr const char* kMortarProfileDirectory = TANK_SOURCE_CONFIG_DIR;
    const std::filesystem::path kInputDevicesPath =
        std::filesystem::path(TANK_SOURCE_CONFIG_DIR) / "input_devices.json";

    const char* RollingTraceEventName(Tank::Physics::RollingTraceEvent event)
    {
        switch (event)
        {
        case Tank::Physics::RollingTraceEvent::StartLatched:
            return "StartLatched";
        case Tank::Physics::RollingTraceEvent::ContinueForward:
            return "ContinueForward";
        case Tank::Physics::RollingTraceEvent::ReturnToStart:
            return "ReturnToStart";
        case Tank::Physics::RollingTraceEvent::Finished:
            return "Finished";
        default:
            return "None";
        }
    }

    Tank::Physics::MapSpawn ManifestSpawn(const Tank::Map::Manifest& manifest)
    {
        constexpr float degreesToRadians = 3.14159265358979323846f / 180.0f;
        Tank::Physics::MapSpawn spawn;
        spawn.position = { manifest.playerSpawn.position[0], manifest.playerSpawn.position[1],
            manifest.playerSpawn.position[2] };
        spawn.yawRadians = manifest.playerSpawn.rotationDegrees[1] * degreesToRadians;
        return spawn;
    }

    Tank::Physics::Vec3 ForwardFromRotation(const Tank::Physics::Quat& rotation)
    {
        return {
            2.0f * (rotation.x * rotation.z + rotation.y * rotation.w),
            2.0f * (rotation.y * rotation.z - rotation.x * rotation.w),
            1.0f - 2.0f * (rotation.x * rotation.x + rotation.y * rotation.y) };
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
    const std::vector<std::string> nodeNames =
        Engine::GetGltfMeshNodeNames(m_tankModelAsset);
    if (nodeNames.size() == 1)
    {
        m_showGltfBody = true;
        m_showGltfCannon = false;
        m_showGltfSide = false;
    }
    m_tankModelLoadStatus = "Loaded: " + path.filename().string();
    return true;
}

void TrackedVehicleMode::SelectMap(Tank::Physics::MapId mapId)
{
    m_customMap.reset();
    m_manifestMap.reset();
    m_manifestHitMeshes.clear();
    m_manifestMissingVisuals = false;
    m_mapHitMeshOverlayInstance.reset();
    m_mapVisualInstances.clear();
    m_mapVisualWorlds.clear();
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
    m_manifestMissingVisuals = false;
    m_mapHitMeshOverlayInstance.reset();
    m_mapVisualInstances.clear();
    m_mapVisualWorlds.clear();
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
    bool missingVisuals = false;
    meshes.reserve(manifest.instances.size());
    for (const Tank::Map::Instance& instance : manifest.instances)
    {
        const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
        Tank::Map::GltfRoles roles;
        if (!Tank::Map::InspectGltfRoles(
            folder / std::filesystem::path(assetUtf8), roles, error))
        {
            error = "Cannot inspect mesh roles '" + instance.asset + "': " + error;
            return false;
        }
        missingVisuals |= std::none_of(roles.meshNodes.begin(), roles.meshNodes.end(),
            [](const Tank::Map::RoleMeshNode& node)
            { return node.role == Tank::Map::MeshRole::Visual; });
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
    m_manifestMissingVisuals = missingVisuals;
    m_mapHitMeshOverlayInstance.reset();
    m_mapVisualInstances.clear();
    m_mapVisualWorlds.clear();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_mapCleared = false;
    m_clearedAreaName.clear();
    m_environmentSettings = {};
    m_activeMapName = folder.filename().string();
    if (missingVisuals)
    {
        m_mapLoadStatus = "Warning: Visual Mesh missing for " + m_activeMapName;
    }
    else
        m_mapLoadStatus = "Loaded Manifest map: " + m_activeMapName;
    error.clear();
    return true;
}

Tank::Input::InputDeviceProfile* TrackedVehicleMode::ActiveInputDeviceProfile()
{
    if (!m_activeInputDeviceProfile ||
        *m_activeInputDeviceProfile >= m_inputDeviceProfiles.profiles.size())
    {
        return nullptr;
    }
    return &m_inputDeviceProfiles.profiles[*m_activeInputDeviceProfile];
}

float TrackedVehicleMode::NormalizeRawGamepadAxis(
    float value,
    float neutral,
    float deadzone)
{
    neutral = std::clamp(neutral, 0.01f, 0.99f);
    const float normalized = value >= neutral
        ? (value - neutral) / (1.0f - neutral)
        : (value - neutral) / neutral;
    const float clampedDeadzone = std::clamp(deadzone, 0.0f, 0.95f);
    if (std::abs(normalized) <= clampedDeadzone) return 0.0f;
    const float magnitude =
        (std::abs(normalized) - clampedDeadzone) / (1.0f - clampedDeadzone);
    return std::copysign(std::min(magnitude, 1.0f), normalized);
}

bool TrackedVehicleMode::Enter(RtPbrSurvey::SceneRenderer& renderer)
{
    m_mapHitMeshOverlayInstance.reset();
    m_mapVisualInstances.clear();
    m_mapVisualWorlds.clear();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    const std::vector<Tank::Physics::MapPrimitive> mapPrimitives = m_manifestMap ?
        std::vector<Tank::Physics::MapPrimitive>{} :
        (m_customMap ? m_customMap->primitives :
            Tank::Physics::BuildMapPrimitives(m_selectedMap, m_environmentSettings));
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
        const uint32_t mapMaterial = m_presenter.SceneBuilder().AddSolidColorMaterial(80, 80, 80, 255);
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
            *m_manifestMap, mapMaterial, error, nullptr, &m_mapVisualInstances) ||
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
        m_mapVisualWorlds.reserve(m_mapVisualInstances.size());
        for (const size_t index : m_mapVisualInstances)
            m_mapVisualWorlds.push_back(scene.instances[index].world);
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
    InitializeDestructibleTargets();
    m_presenter.AppendDestructibleBoxes(m_test.State());
    ApplyAssaultProjectileSettings();
    m_presenter.EnsureImpactMarkCapacity(m_test.State().assaultImpactMarks.Capacity());
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

void TrackedVehicleMode::Exit(RtPbrSurvey::SceneRenderer& renderer)
{
    for (const auto handle : m_assaultTracerLines) renderer.RemoveDebugLine(handle);
    m_assaultTracerLines.clear();
    m_presenter.Clear();
    m_mapHitMeshOverlayInstance.reset();
    m_mapVisualInstances.clear();
    m_mapVisualWorlds.clear();
    m_mapMarkerInstances.clear();
    m_mapMarkerWorlds.clear();
    m_mapClearBeaconInstances.clear();
    m_mapClearBeaconWorlds.clear();
    m_clearedAreaIndex.reset();
    m_active = false;
}

void TrackedVehicleMode::UpdateSceneInternal(RtPbrSurvey::SceneRenderer& renderer)
{
    const Tank::Physics::TrackedVehicleTestState& state = m_test.State();
    const bool impactMarkPoolChanged = m_presenter.EnsureImpactMarkCapacity(state.assaultImpactMarks.Capacity());
    if (state.assaultWeapon.roundsFired != m_lastAssaultRoundsFired)
    {
        m_lastAssaultRoundsFired = state.assaultWeapon.roundsFired;
        char message[256] = {};
        std::snprintf(
            message,
            sizeof(message),
            "[Tank DebugLine] Assault fired: round=%llu body=(%.2f, %.2f, %.2f)\n",
            static_cast<unsigned long long>(m_lastAssaultRoundsFired),
            state.bodyPosition.x,
            state.bodyPosition.y,
            state.bodyPosition.z);
        Tank::Diagnostics::Write(Tank::Diagnostics::LogLevel::Debug, "Vehicle", message);
    }
    const size_t maximumLines = static_cast<size_t>(m_test.ProjectileSettings().maximumCount);
    while (m_assaultTracerLines.size() > maximumLines)
    {
        renderer.RemoveDebugLine(m_assaultTracerLines.back());
        m_assaultTracerLines.pop_back();
    }
    m_assaultTracerLines.resize((std::max)(m_assaultTracerLines.size(), state.assaultProjectiles.size()),
        RtPbrSurvey::kInvalidDebugLineHandle);
    for (size_t i = 0; i < m_assaultTracerLines.size(); ++i)
    {
        RtPbrSurvey::DebugLineDesc tracer;
        tracer.color = {1.0f, 0.8f, 0.15f, 1.0f};
        tracer.depthMode = RtPbrSurvey::DebugLineDepthMode::Overlay;
        tracer.visible = i < state.assaultProjectiles.size();
        if (tracer.visible)
        {
            const auto& projectile = state.assaultProjectiles[i];
            const auto& v = projectile.velocity;
            const float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            const float trailSeconds = (std::min)(projectile.ageSeconds, 1.5f / speed);
            tracer.start = {projectile.position.x - v.x * trailSeconds,
                projectile.position.y - v.y * trailSeconds, projectile.position.z - v.z * trailSeconds};
            tracer.end = {projectile.position.x, projectile.position.y, projectile.position.z};
        }
        auto& handle = m_assaultTracerLines[i];
        if (handle == RtPbrSurvey::kInvalidDebugLineHandle || !renderer.UpdateDebugLine(handle, tracer))
            handle = renderer.AddDebugLine(tracer);
    }

    const auto cue = MortarRangeCue();
    if (cue.visible != m_lastLoggedMortarVisible)
    {
        char message[192] = {};
        std::snprintf(
            message,
            sizeof(message),
            "[Tank DebugLine] Mortar visibility changed: visible=%d canFire=%d radius=%.2f\n",
            cue.visible ? 1 : 0,
            cue.canFire ? 1 : 0,
            cue.radiusMeters);
        Tank::Diagnostics::Write(Tank::Diagnostics::LogLevel::Debug, "Vehicle", message);
        m_lastLoggedMortarVisible = cue.visible;
    }
    m_presenter.SetMortarRangeCue(cue);
    const auto vertices = Tank::Rendering::MortarRangeGeometry::BuildCircle(
        cue.center, cue.radiusMeters, 32);
    for (size_t i = 0; i < m_mortarRangeLines.size(); ++i)
    {
        const size_t next = (i + 1) % m_mortarRangeLines.size();
        RtPbrSurvey::DebugLineDesc line;
        if (vertices.size() == m_mortarRangeLines.size())
        {
            line.start = {vertices[i].x, vertices[i].y, vertices[i].z};
            line.end = {vertices[next].x, vertices[next].y, vertices[next].z};
        }
        line.visible = cue.visible;
        line.color = cue.canFire
            ? DirectX::XMFLOAT4(1.0f, 0.2f, 0.1f, 1.0f)
            : DirectX::XMFLOAT4(1.0f, 0.8f, 0.1f, 1.0f);
        line.depthMode = RtPbrSurvey::DebugLineDepthMode::Overlay;
        if (m_mortarRangeLines[i] == RtPbrSurvey::kInvalidDebugLineHandle)
            m_mortarRangeLines[i] = renderer.AddDebugLine(line);
        else if (!renderer.UpdateDebugLine(m_mortarRangeLines[i], line))
        {
            char message[160] = {};
            std::snprintf(
                message,
                sizeof(message),
                "[Tank DebugLine] Mortar line update failed: index=%zu handle=%u\n",
                i,
                m_mortarRangeLines[i]);
            Tank::Diagnostics::Write(Tank::Diagnostics::LogLevel::Warning, "Vehicle", message);
        }
    }
    m_presenter.UpdateScene(
        state,
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
    if (impactMarkPoolChanged)
    {
        renderer.ReloadSceneResources(m_presenter.GetScene());
        renderer.SetDisplayInstanceCount(static_cast<int>(m_presenter.GetScene().instances.size()));
    }
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
    for (size_t visual = 0; visual < m_mapVisualInstances.size(); ++visual)
    {
        const size_t instanceIndex = m_mapVisualInstances[visual];
        if (instanceIndex >= m_presenter.GetScene().instances.size()) continue;
        Engine::InstanceData& instance = m_presenter.GetScene().instances[instanceIndex];
        instance.prevWorld = instance.world;
        if (m_mapVisualMeshes)
            instance.world = m_mapVisualWorlds[visual];
        else
            DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixTranspose(
                DirectX::XMMatrixScaling(0.0f, 0.0f, 0.0f)));
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
    bool rollLeft, bool rollRight, bool brake, bool fireAssault,
    bool mortar)
{
    Tank::Physics::TankInput input;
    m_activeInputDeviceProfile.reset();
    for (size_t index = 0; index < m_inputDeviceProfiles.profiles.size(); ++index)
    {
        const Tank::Input::InputDeviceProfile& candidate =
            m_inputDeviceProfiles.profiles[index];
        if (candidate.vendorId == gamepadState.vendorId &&
            candidate.productId == gamepadState.productId)
        {
            m_activeInputDeviceProfile = index;
            break;
        }
    }
    Tank::Input::InputDeviceProfile* profile = ActiveInputDeviceProfile();
    const auto hasAxis = [&gamepadState](size_t axis)
    {
        return axis < gamepadState.axisCount && axis < gamepadState.rawAxes.size();
    };
    m_analogTracksConnected = gamepadState.connected && profile != nullptr &&
        hasAxis(profile->leftTrackAxis) && hasAxis(profile->rightTrackAxis) &&
        hasAxis(profile->leftRollAxis) && hasAxis(profile->rightRollAxis);
    if (!m_analogTracksConnected)
    {
        m_analogTracksArmed = false;
    }
    else if (!m_analogTracksArmed)
    {
        const bool tracksNeutral =
            std::abs(gamepadState.rawAxes[profile->leftTrackAxis] - profile->neutral) <=
                profile->neutralTolerance &&
            std::abs(gamepadState.rawAxes[profile->rightTrackAxis] - profile->neutral) <=
                profile->neutralTolerance;
        m_analogTracksArmed = tracksNeutral;
    }
    const bool useAnalogTracks = m_analogTracksConnected && m_analogTracksArmed;
    const bool profileBrake = profile != nullptr &&
        gamepadState.IsRawButtonPressed(profile->brakeButton);
    const bool standardGamepadBrake =
        gamepadState.hasGamepadMapping && gamepadState.brakePressed;
    const bool brakePressed = brake || profileBrake || standardGamepadBrake;
    const bool gamepadAssaultButton = profile != nullptr &&
        gamepadState.IsRawButtonPressed(profile->fireButton);
    input.fireAssault = fireAssault || gamepadAssaultButton ||
        (gamepadState.hasGamepadMapping && gamepadState.rightTrigger >= 0.5f);

    const auto mappedAxis = [&](size_t axis, bool inverted)
    {
        if (!useAnalogTracks) return 0.0f;
        const float value = NormalizeRawGamepadAxis(
            gamepadState.rawAxes[axis], profile->neutral, profile->deadzone);
        return inverted ? -value : value;
    };
    m_analogLeftTrack = mappedAxis(profile != nullptr ? profile->leftTrackAxis : 0,
        profile != nullptr && profile->invertLeftTrack);
    m_analogRightTrack = mappedAxis(profile != nullptr ? profile->rightTrackAxis : 0,
        profile != nullptr && profile->invertRightTrack);
    const float analogRollAxis0 = mappedAxis(
        profile != nullptr ? profile->leftRollAxis : 0,
        profile != nullptr && profile->invertLeftRoll);
    const float analogRollAxis2 = mappedAxis(
        profile != nullptr ? profile->rightRollAxis : 0,
        profile != nullptr && profile->invertRightRoll);

    // axis 0 is the physical left lever X axis and axis 2 is the physical
    // right lever X axis. Keep this mapping in lever-local coordinates:
    // left outward = negative, right outward = positive.
    input.leftLeverX = analogRollAxis0;
    input.rightLeverX = analogRollAxis2;

    if (!useAnalogTracks && rollLeft != rollRight)
    {
        const float keyboardLeverX = rollLeft ? -1.0f : 1.0f;
        input.leftLeverX = keyboardLeverX;
        input.rightLeverX = keyboardLeverX;
    }

    if (mortar)
    {
        input.leftLeverX = -1.0f;
        input.rightLeverX = 1.0f;
    }

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
            standardGamepadBrake);
    if (gamepadActive)
    {
        const bool keyboardBrake = input.brake;
        const float keyboardRoll = input.roll;
        const bool keyboardOrTriggerFire = input.fireAssault;
        input = Tank::Input::MapGamepadToTankInput(gamepadState, m_inputMappingSettings);
        input.brake = input.brake || keyboardBrake;
        input.roll = keyboardRoll;
        input.fireAssault = input.fireAssault || keyboardOrTriggerFire;
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
    const int previousMaximum = m_test.ProjectileSettings().maximumCount;
    const int previousMarkMaximum = m_test.ProjectileSettings().maximumImpactMarks;
    ApplyAssaultProjectileSettings();
    if (m_paused && !m_singleStep && (previousMaximum != m_test.ProjectileSettings().maximumCount ||
        previousMarkMaximum != m_test.ProjectileSettings().maximumImpactMarks))
        UpdateSceneInternal(renderer);
    if (!m_paused || m_singleStep)
    {
        const auto physicsStart = std::chrono::steady_clock::now();
        m_test.Step(kPhysicsFixedDt);
        const Tank::Physics::TrackedVehicleTestState& state = m_test.State();
        if (state.rollingTraceSequence != m_loggedRollingTraceSequence)
        {
            char message[192] = {};
            std::snprintf(
                message,
                sizeof(message),
                "[Tank Rolling] seq=%llu event=%s phase=%d request=%+.0f command=%+.0f input=%+.0f angularZ=%+.3f\n",
                static_cast<unsigned long long>(state.rollingTraceSequence),
                RollingTraceEventName(state.lastRollingTraceEvent),
                static_cast<int>(state.rollingPhase),
                state.rollingTraceRequestSign,
                state.rollingTraceCommandSign,
                state.rollingTraceInputSign,
                state.angularVelocity.z);
            Tank::Diagnostics::Write(Tank::Diagnostics::LogLevel::Debug, "Vehicle", message);
            m_loggedRollingTraceSequence = state.rollingTraceSequence;
        }
        const auto sceneStart = std::chrono::steady_clock::now();
        m_physicsStepTimeMs = std::chrono::duration<float, std::milli>(
            sceneStart - physicsStart).count();
        m_physicsStepPeakTimeMs = (std::max)(
            m_physicsStepPeakTimeMs,
            m_physicsStepTimeMs);
        UpdateClearCondition();
        UpdateSceneInternal(renderer);
        renderer.SetScene(m_presenter.GetScene());
        m_sceneUpdateTimeMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - sceneStart).count();
        m_sceneUpdatePeakTimeMs = (std::max)(
            m_sceneUpdatePeakTimeMs,
            m_sceneUpdateTimeMs);
        m_singleStep = false;
    }

    if (Engine::CameraState* camera = ActiveCamera())
    {
        const auto& aim = m_test.State().mortarAim;
        const float progress = aim.atMaximum ? 1.0f :
            (aim.canFire ? 0.5f : 0.0f);
        cameraController.SetMortarCameraCue(
            Tank::Physics::MortarCameraCue::FromWheelieProgress(progress));
        cameraController.UpdateFollowCamera(
            m_test.State(),
            kPhysicsFixedDt,
            *camera);
    }
}

void TrackedVehicleMode::ResetFrameTimingPeaks()
{
    m_physicsStepPeakTimeMs = m_physicsStepTimeMs;
    m_sceneUpdatePeakTimeMs = m_sceneUpdateTimeMs;
}

void TrackedVehicleMode::ApplyAssaultProjectileSettings()
{
    const size_t baseInstances = m_presenter.GetScene().instances.size() - m_presenter.ImpactMarkInstanceCount();
    const size_t limit = RtPbrSurveyEngine::kMaxInstanceCount;
    const int available = static_cast<int>(baseInstances < limit ? limit - baseInstances : 0);
    auto& settings = m_settings.assaultProjectiles;
    settings.maximumImpactMarks = std::clamp(settings.maximumImpactMarks, 0, available);
    m_test.SetAssaultProjectileSettings(settings);
}

void TrackedVehicleMode::InitializeDestructibleTargets()
{
    // Test targets are opt-in for the flat sandbox, not authored maps or courses.
    if (m_manifestMap || m_customMap || m_selectedMap != Tank::Physics::MapId::FlatGround) return;
    m_test.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    m_test.AddDestructibleBox({-4.0f, 1.5f, 16.0f}, {2.0f, 3.0f, 2.0f});
    m_test.AddDestructibleBox({4.0f, 1.5f, 16.0f}, {2.0f, 3.0f, 2.0f});
}

void TrackedVehicleMode::Reset(
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    const Tank::Physics::TrackedVehicleTestState previousState = m_test.State();
    Engine::CameraState* camera = ActiveCamera();
    const std::vector<Tank::Physics::MapPrimitive> mapPrimitives = m_manifestMap ?
        std::vector<Tank::Physics::MapPrimitive>{} :
        (m_customMap ? m_customMap->primitives :
            Tank::Physics::BuildMapPrimitives(
                m_selectedMap,
                m_environmentSettings));
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
    InitializeDestructibleTargets();
    ApplyAssaultProjectileSettings();
    m_appliedSettings = m_settings;
    m_appliedEnvironmentSettings = m_environmentSettings;
    m_singleStep = false;
    m_analogTracksArmed = false;
    m_loggedRollingTraceSequence = 0;
    m_lastAssaultRoundsFired = 0;
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

bool TrackedVehicleMode::FireAssault()
{
    ApplyAssaultProjectileSettings();
    const bool fired = m_test.FireAssault();
    if (fired && m_paused)
    {
        m_singleStep = true;
    }
    return fired;
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

Tank::Rendering::MortarRangeCue TrackedVehicleMode::MortarRangeCue() const
{
    const auto& state = m_test.State();
    Tank::Rendering::MortarRangeCue cue;
    const Tank::Physics::Vec3 forward = ForwardFromRotation(state.bodyRotation);
    cue.center = {
        state.bodyPosition.x + forward.x * state.mortarAim.rangeMeters,
        0.02f,
        state.bodyPosition.z + forward.z * state.mortarAim.rangeMeters};
    cue.radiusMeters = state.mortarAim.attackRadiusMeters;
    const auto& input = m_test.Input();
    const bool held = input.leftLeverX <= -0.70f && input.rightLeverX >= 0.70f;
    cue.visible = held &&
        (state.specialMove.state == Tank::Physics::SpecialMoveState::MortarStarting ||
            state.specialMove.state == Tank::Physics::SpecialMoveState::MortarAiming);
    cue.canFire = state.mortarAim.canFire;
    return cue;
}

bool TrackedVehicleMode::SaveTankSettings()
{
    Tank::App::TankSettingsStore store(
        m_tankSettingsSlot,
        kTankSettingsDirectory);
    return store.Write(m_settings, m_tankSettingsStatus);
}

bool TrackedVehicleMode::SaveRollingProfile()
{
    Tank::App::RollingProfileStore store(
        m_rollingProfileSlot,
        kRollingProfileDirectory);
    return store.Write(
        Tank::Physics::ExtractRollingProfile(m_settings),
        m_rollingProfileStatus);
}

bool TrackedVehicleMode::SaveMortarProfile()
{
    Tank::App::MortarProfileStore store(m_mortarProfileSlot, kMortarProfileDirectory);
    return store.Write(Tank::Physics::ExtractMortarProfile(m_settings), m_mortarProfileStatus);
}

bool TrackedVehicleMode::SaveInputMappingSettings()
{
    std::ofstream file(kInputDevicesPath);
    if (!file)
    {
        m_inputMappingStatus = "Save failed: " + kInputDevicesPath.string();
        return false;
    }
    file << Tank::Input::SaveInputDeviceProfiles(m_inputDeviceProfiles).dump(4);
    if (!file)
    {
        m_inputMappingStatus = "Save failed: " + kInputDevicesPath.string();
        return false;
    }
    m_inputMappingStatus = "Saved: " + kInputDevicesPath.string();
    return true;
}

bool TrackedVehicleMode::LoadInputMappingSettings()
{
    std::ifstream file(kInputDevicesPath);
    if (!file)
    {
        m_inputMappingStatus = "Load failed: " + kInputDevicesPath.string();
        return false;
    }
    nlohmann::json json;
    file >> json;
    m_inputDeviceProfiles = Tank::Input::LoadInputDeviceProfiles(json);
    m_activeInputDeviceProfile.reset();
    m_inputMappingStatus = "Loaded: " + kInputDevicesPath.string();
    return true;
}

bool TrackedVehicleMode::LoadTankSettings(
    bool apply,
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    Tank::App::TankSettingsStore store(
        m_tankSettingsSlot,
        kTankSettingsDirectory);
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

bool TrackedVehicleMode::LoadRollingProfile(
    bool apply,
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    Tank::App::RollingProfileStore store(
        m_rollingProfileSlot,
        kRollingProfileDirectory);
    Tank::Physics::RollingProfile profile =
        Tank::Physics::ExtractRollingProfile(m_settings);
    if (!store.Read(profile, m_rollingProfileStatus))
    {
        return false;
    }
    Tank::Physics::ApplyRollingProfile(profile, m_settings);
    if (apply)
    {
        Reset(renderer, cameraController);
    }
    return true;
}

bool TrackedVehicleMode::LoadMortarProfile(
    bool apply,
    RtPbrSurvey::SceneRenderer& renderer,
    Tank::App::CameraController& cameraController)
{
    Tank::App::MortarProfileStore store(m_mortarProfileSlot, kMortarProfileDirectory);
    Tank::Physics::MortarProfile profile = Tank::Physics::ExtractMortarProfile(m_settings);
    if (!store.Read(profile, m_mortarProfileStatus)) return false;
    Tank::Physics::ApplyMortarProfile(profile, m_settings);
    if (apply) Reset(renderer, cameraController);
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
