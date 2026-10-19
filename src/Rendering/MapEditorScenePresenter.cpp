#include "MapEditorScenePresenter.h"

#include "Map/GltfRoles.h"
#include "Map/MapHierarchy.h"
#include "Map/GltfHitMesh.h"
#include "MapVisualLoader.h"

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <Scene/CameraView.h>
#include <Scene/CameraProjection.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace
{
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string value = path.u8string();
        return { value.begin(), value.end() };
    }

    DirectX::XMMATRIX ToWorld(const Tank::Map::Transform& transform)
    {
        if (transform.worldMatrix)
        {
            DirectX::XMFLOAT4X4 matrix;
            std::copy(transform.worldMatrix->begin(), transform.worldMatrix->end(), &matrix._11);
            return DirectX::XMLoadFloat4x4(&matrix);
        }
        constexpr float degreesToRadians = DirectX::XM_PI / 180.0f;
        return DirectX::XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
            DirectX::XMMatrixRotationRollPitchYaw(
                   transform.rotationDegrees[0] * degreesToRadians,
                   transform.rotationDegrees[1] * degreesToRadians,
                   transform.rotationDegrees[2] * degreesToRadians) *
            DirectX::XMMatrixTranslation(
                transform.position[0], transform.position[1], transform.position[2]);
    }

    void AppendSelectionWireframe(Engine::SceneBuilder& builder, Engine::SceneMeshId cubeMesh,
        const Tank::Map::Instance& instance, const Tank::Rendering::MapVisualBounds& bounds,
        uint32_t materialId)
    {
        std::array<float, 3> center = {};
        std::array<float, 3> size = {};
        float largestExtent = 0.0f;
        for (size_t axis = 0; axis < 3; ++axis)
        {
            center[axis] = (bounds.minimum[axis] + bounds.maximum[axis]) * 0.5f;
            size[axis] = bounds.maximum[axis] - bounds.minimum[axis];
            largestExtent = (std::max)(largestExtent, size[axis]);
        }
        const float padding = (std::max)(0.03f, largestExtent * 0.015f);
        for (float& extent : size) extent = (std::max)(extent, padding) + padding * 2.0f;
        const float thickness = (std::clamp)(largestExtent * 0.012f, 0.025f, 0.12f);
        const float halfX = size[0] * 0.5f;
        const float halfY = size[1] * 0.5f;
        const float halfZ = size[2] * 0.5f;
        const DirectX::XMMATRIX world = ToWorld(instance.transform);
        const auto addEdge = [&](float sx, float sy, float sz, float px, float py, float pz)
        {
            builder.AddInstance(cubeMesh,
                DirectX::XMMatrixScaling(sx, sy, sz) *
                    DirectX::XMMatrixTranslation(
                        center[0] + px, center[1] + py, center[2] + pz) * world,
                materialId);
        };
        for (const float y : { -halfY, halfY })
            for (const float z : { -halfZ, halfZ })
                addEdge(size[0], thickness, thickness, 0.0f, y, z);
        for (const float x : { -halfX, halfX })
            for (const float z : { -halfZ, halfZ })
                addEdge(thickness, size[1], thickness, x, 0.0f, z);
        for (const float x : { -halfX, halfX })
            for (const float y : { -halfY, halfY })
                addEdge(thickness, thickness, size[2], x, y, 0.0f);
    }

    Tank::Rendering::MapEditorFocusTarget MakeFocusTarget(
        const Tank::Map::Instance& instance, const Tank::Rendering::MapVisualBounds& bounds)
    {
        const DirectX::XMVECTOR localCenter = DirectX::XMVectorSet(
            (bounds.minimum[0] + bounds.maximum[0]) * 0.5f,
            (bounds.minimum[1] + bounds.maximum[1]) * 0.5f,
            (bounds.minimum[2] + bounds.maximum[2]) * 0.5f, 1.0f);
        DirectX::XMFLOAT3 worldCenter = {};
        DirectX::XMStoreFloat3(&worldCenter,
            DirectX::XMVector3TransformCoord(localCenter, ToWorld(instance.transform)));
        const float halfX = (bounds.maximum[0] - bounds.minimum[0]) * 0.5f;
        const float halfY = (bounds.maximum[1] - bounds.minimum[1]) * 0.5f;
        const float halfZ = (bounds.maximum[2] - bounds.minimum[2]) * 0.5f;
        return { { worldCenter.x, worldCenter.y, worldCenter.z },
            (std::max)(0.5f, (std::max)({ instance.transform.scale[0], instance.transform.scale[1],
                instance.transform.scale[2] }) * std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ)) };
    }

    Tank::Rendering::MapVisualBounds MakeHitBounds(
        const std::string& instanceId, const Tank::Map::HitTriangleMesh& mesh)
    {
        Tank::Rendering::MapVisualBounds bounds;
        bounds.instanceId = instanceId;
        bounds.minimum.fill((std::numeric_limits<float>::max)());
        bounds.maximum.fill((std::numeric_limits<float>::lowest)());
        for (const std::array<float, 3>& vertex : mesh.vertices)
        {
            for (size_t axis = 0; axis < 3; ++axis)
            {
                bounds.minimum[axis] = (std::min)(bounds.minimum[axis], vertex[axis]);
                bounds.maximum[axis] = (std::max)(bounds.maximum[axis], vertex[axis]);
            }
        }
        return bounds;
    }

    bool FindMissingVisuals(const std::filesystem::path& mapFolder,
        const Tank::Map::Manifest& manifest, std::unordered_set<std::string>& instanceIds,
        std::vector<std::string>& assets, std::string& error)
    {
        for (const Tank::Map::Instance& instance : manifest.instances)
        {
            const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
            Tank::Map::GltfRoles roles;
            if (!Tank::Map::InspectGltfRoles(
                mapFolder / std::filesystem::path(assetUtf8), roles, error))
                return false;
            const bool hasVisual = std::any_of(roles.meshNodes.begin(), roles.meshNodes.end(),
                [](const Tank::Map::RoleMeshNode& node)
                { return node.role == Tank::Map::MeshRole::Visual; });
            if (!hasVisual)
            {
                instanceIds.insert(instance.id);
                if (std::find(assets.begin(), assets.end(), instance.asset) == assets.end())
                    assets.push_back(instance.asset);
            }
        }
        return true;
    }

}

bool Tank::Rendering::MapEditorScenePresenter::Rebuild(
    const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
    const MapEditorGridSettings& grid, std::string& error)
{
    return Rebuild(mapFolder, manifest, grid, {}, error);
}

bool Tank::Rendering::MapEditorScenePresenter::Rebuild(
    const std::filesystem::path& mapFolder, const Map::Manifest& localManifest,
    const MapEditorGridSettings& grid, const MapEditorPreviewSettings& preview,
    std::string& error)
{
    Map::Manifest manifest;
    if (!Map::ResolveHierarchy(localManifest, manifest, error, true)) return false;
    auto next = std::make_unique<Engine::SceneBuilder>();
    std::vector<PickRange> pickRanges;
    const auto addPickInstance = [&](size_t index, const std::string& nodeId)
    {
        const auto& range = next->GetMesh().ranges[next->GetScene().instances[index].meshId];
        pickRanges.push_back({nodeId, index, range.firstIndex, range.indexCount});
    };
    const auto colorByte = [](float value)
    {
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    const uint32_t fallbackMaterial = next->AddSolidColorMaterial(
        colorByte(preview.visualMeshColor[0]), colorByte(preview.visualMeshColor[1]),
        colorByte(preview.visualMeshColor[2]), 255);
    const uint32_t gridMaterial = next->AddSolidColorMaterial(100, 150, 185, 255);
    const uint32_t xAxisMaterial = next->AddSolidColorMaterial(210, 85, 85, 255);
    const uint32_t zAxisMaterial = next->AddSolidColorMaterial(85, 190, 105, 255);
    const uint32_t spawnMaterial = next->AddSolidColorMaterial(255, 190, 35, 255);
    const uint32_t clearAreaMaterial = next->AddSolidColorMaterial(35, 225, 235, 255);
    const uint32_t selectionMaterial = next->AddSolidColorMaterial(255, 220, 35, 255);
    const uint32_t hitMeshMaterial = next->AddSolidColorMaterial(255, 45, 190, 255);
    const Engine::SceneMeshId gridMesh = next->AddCube(1.0f);
    const float extent = grid.spacingMeters * static_cast<float>(grid.halfCellCount);
    constexpr float gridSurfaceOffsetMeters = 0.002f;
    const float gridCenterY = gridSurfaceOffsetMeters - grid.lineWidthMeters * 0.5f;
    for (int cell = -grid.halfCellCount; grid.visible && cell <= grid.halfCellCount; ++cell)
    {
        const float offset = grid.spacingMeters * static_cast<float>(cell);
        const uint32_t xAxisLineMaterial = cell == 0 ? xAxisMaterial : gridMaterial;
        const uint32_t zAxisLineMaterial = cell == 0 ? zAxisMaterial : gridMaterial;
        next->AddInstance(gridMesh,
            DirectX::XMMatrixScaling(extent * 2.0f, grid.lineWidthMeters, grid.lineWidthMeters) *
                DirectX::XMMatrixTranslation(0.0f, gridCenterY, offset), zAxisLineMaterial);
        next->AddInstance(gridMesh,
            DirectX::XMMatrixScaling(grid.lineWidthMeters, grid.lineWidthMeters, extent * 2.0f) *
                DirectX::XMMatrixTranslation(offset, gridCenterY, 0.0f), xAxisLineMaterial);
    }
    Map::Manifest visibleManifest = manifest;
    std::erase_if(visibleManifest.instances, [&preview](const Map::Instance& instance)
        { return preview.hiddenInstanceIds.contains(instance.id); });
    std::unordered_set<std::string> missingVisualIds;
    std::vector<std::string> missingVisualAssets;
    if (!FindMissingVisuals(mapFolder, manifest, missingVisualIds, missingVisualAssets, error))
        return false;
    std::vector<MapVisualBounds> visualBounds;
    std::vector<std::pair<size_t, std::string>> visualPickInstances;
    if (preview.showVisualMeshes &&
        !AppendMapVisuals(*next, mapFolder, visibleManifest, fallbackMaterial, error, &visualBounds, nullptr, &visualPickInstances))
        return false;
    for (const auto& [index, nodeId] : visualPickInstances) addPickInstance(index, nodeId);
    std::vector<Map::HitTriangleMesh> hitMeshes;
    std::vector<std::string> hitNodeIds;
    std::vector<MapVisualBounds> hitBounds;
    for (const Map::Instance& instance : visibleManifest.instances)
    {
        const bool fallback = preview.showVisualMeshes && missingVisualIds.contains(instance.id);
        if (!preview.showHitMeshes && !fallback) continue;
        const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
        Map::HitTriangleMesh local;
        if (!Map::LoadGltfHitMesh(mapFolder / std::filesystem::path(assetUtf8), local, error))
            return false;
        hitBounds.push_back(MakeHitBounds(instance.id, local));
        Map::HitTriangleMesh world;
        if (!Map::TransformHitMesh(local, instance.transform, world, error))
            return false;
        hitMeshes.push_back(std::move(world));
        hitNodeIds.push_back("mesh/" + instance.id);
    }
    if (!hitMeshes.empty())
    {
        size_t hitMeshInstance = 0;
        if (!AppendMapHitMeshOverlay(*next, hitMeshes, hitMeshMaterial, hitMeshInstance, error))
            return false;
        Engine::InstanceData& instance = next->GetScene().instances[hitMeshInstance];
        DirectX::XMStoreFloat4x4(&instance.world, DirectX::XMMatrixIdentity());
        instance.prevWorld = instance.world;
        uint32_t firstIndex = next->GetMesh().ranges[instance.meshId].firstIndex;
        for (size_t i = 0; i < hitMeshes.size(); ++i)
        {
            const auto count = static_cast<uint32_t>(hitMeshes[i].triangles.size() * 3);
            pickRanges.push_back({hitNodeIds[i], hitMeshInstance, firstIndex, count});
            firstIndex += count;
        }
    }
    const auto selectedInstance = std::find_if(visibleManifest.instances.begin(), visibleManifest.instances.end(),
        [&preview](const Map::Instance& instance) { return instance.id == preview.selectedInstanceId; });
    const auto selectedBounds = std::find_if(visualBounds.begin(), visualBounds.end(),
        [&preview](const MapVisualBounds& bounds) { return bounds.instanceId == preview.selectedInstanceId; });
    const auto selectedHitBounds = std::find_if(hitBounds.begin(), hitBounds.end(),
        [&preview](const MapVisualBounds& bounds) { return bounds.instanceId == preview.selectedInstanceId; });
    std::optional<MapEditorFocusTarget> selectedFocusTarget;
    const MapVisualBounds* selectionBounds = selectedBounds != visualBounds.end() ?
        &*selectedBounds : selectedHitBounds != hitBounds.end() ? &*selectedHitBounds : nullptr;
    if (selectedInstance != visibleManifest.instances.end() && selectionBounds != nullptr)
    {
        AppendSelectionWireframe(*next, gridMesh, *selectedInstance, *selectionBounds, selectionMaterial);
        selectedFocusTarget = MakeFocusTarget(*selectedInstance, *selectionBounds);
    }
    const uint32_t enemyMaterial = next->AddSolidColorMaterial(235, 65, 65, 255);
    for (const auto& enemy : manifest.enemies)
    {
        const size_t firstEnemyInstance = next->GetScene().instances.size();
        const Map::Transform transform = { enemy.position, enemy.rotationDegrees };
        const auto world = ToWorld(transform);
        const bool selected = enemy.id == preview.selectedEnemyId;
        const uint32_t material = selected ? selectionMaterial : enemyMaterial;
        next->AddInstance(gridMesh, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) * world, material);
        next->AddInstance(gridMesh, DirectX::XMMatrixScaling(0.12f, 0.12f, 1.5f) *
            DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f) * world, material);
        next->AddInstance(gridMesh, DirectX::XMMatrixScaling(0.35f, 0.16f, 0.35f) *
            DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4) *
            DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.75f) * world, material);
        for (size_t i = firstEnemyInstance; i < next->GetScene().instances.size(); ++i)
            addPickInstance(i, "enemy/" + enemy.id);
        if (selected) selectedFocusTarget = MapEditorFocusTarget{ enemy.position, 2.0f };
    }
    std::vector<size_t> markerInstances;
    AppendMapMarkers(*next, gridMesh, manifest, spawnMaterial,
        clearAreaMaterial, markerInstances);
    size_t marker = 0;
    const auto addMarkerPicks = [&](const std::string& id, size_t count)
    {
        for (size_t i = 0; i < count; ++i) addPickInstance(markerInstances[marker++], id);
    };
    if (Map::GetNodeInfo(manifest, "player-start").visible)
        addMarkerPicks("player-start", kPlayerStartMarkerPartCount);
    for (const auto& start : manifest.startPoints)
        if (Map::GetNodeInfo(manifest, "start/" + start.id).visible)
            addMarkerPicks("start/" + start.id, kPlayerStartMarkerPartCount);
    for (const auto& area : manifest.clearAreas)
        addMarkerPicks("area/" + area.id, kClearAreaMarkerPartCount);
    next->GetScene().camera.pos = { 8.0f, 6.0f, -8.0f };
    next->GetScene().camera.gazePoint = { 0.0f, 0.0f, 0.0f };
    m_builder = std::move(next);
    m_pickRanges = std::move(pickRanges);
    if (!selectedFocusTarget && !preview.selectedNodeId.empty())
    {
        const auto tree = Map::MapTree(localManifest);
        const auto selected = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == preview.selectedNodeId;});
        if (selected != tree.end())
        {
            const auto transform = Map::NodeWorldTransform(localManifest, selected->id);
            float radius = 1.5f;
            for (const auto& area : manifest.clearAreas)
                if ("area/" + area.id == selected->id)
                    radius = (std::max)(radius, 0.5f * std::sqrt(area.size[0]*area.size[0] + area.size[1]*area.size[1] + area.size[2]*area.size[2]));
            selectedFocusTarget = MapEditorFocusTarget{transform.position, radius};
        }
    }
    m_selectedFocusTarget = selectedFocusTarget;
    if (missingVisualAssets.empty())
        m_warning.clear();
    else
    {
        std::ostringstream warning;
        warning << "No Visual Mesh in ";
        for (size_t index = 0; index < missingVisualAssets.size(); ++index)
        {
            if (index > 0) warning << ", ";
            warning << missingVisualAssets[index];
        }
        warning << ". Showing Hit Mesh as the fallback.";
        m_warning = warning.str();
    }
    error.clear();
    return true;
}

bool Tank::Rendering::MapEditorScenePresenter::ValidateVisualAsset(
    const std::filesystem::path& assetPath, const Map::GltfRoles& roles, std::string& error) const
{
    const Engine::GltfSceneAssetLoadResult asset = Engine::LoadGltfSceneAsset(ToUtf8(assetPath));
    if (!asset)
    {
        error = asset.message;
        return false;
    }
    Engine::SceneBuilder builder;
    bool hasVisual = false;
    for (const Map::RoleMeshNode& node : roles.meshNodes)
    {
        if (node.role != Map::MeshRole::Visual)
            continue;
        hasVisual = true;
        const Engine::GltfNodeMeshAddResult add = builder.AddGltfNodeMesh(asset.asset, static_cast<uint32_t>(node.nodeIndex));
        if (!add)
        {
            error = add.message;
            return false;
        }
    }
    if (!hasVisual)
    {
        Map::HitTriangleMesh hitMesh;
        if (!Map::LoadGltfHitMesh(assetPath, hitMesh, error)) return false;
    }
    error.clear();
    return true;
}

void Tank::Rendering::MapEditorScenePresenter::Clear()
{
    m_builder = std::make_unique<Engine::SceneBuilder>();
    m_selectedFocusTarget.reset();
    m_pickRanges.clear();
    m_warning.clear();
}

std::optional<std::string> Tank::Rendering::MapEditorScenePresenter::PickRay(
    const std::array<float, 3>& origin, const std::array<float, 3>& direction) const
{
    using namespace DirectX;
    for (size_t i = 0; i < 3; ++i)
        if (!std::isfinite(origin[i]) || !std::isfinite(direction[i])) return std::nullopt;
    const auto rayOrigin = XMVectorSet(origin[0], origin[1], origin[2], 1);
    const auto rawDirection = XMVectorSet(direction[0], direction[1], direction[2], 0);
    if (XMVectorGetX(XMVector3LengthSq(rawDirection)) < 1.0e-12f) return std::nullopt;
    const auto rayDirection = XMVector3Normalize(rawDirection);
    const auto& scene = m_builder->GetScene();
    if (!scene.mesh) return std::nullopt;
    const auto& mesh = *scene.mesh;
    float closest = (std::numeric_limits<float>::max)();
    std::optional<std::string> result;
    for (const auto& target : m_pickRanges)
    {
        const auto& instance = scene.instances[target.instanceIndex];
        if (!Engine::HasRenderableTransform(instance)) continue;
        // SceneBuilder stores matrices transposed for the GPU.
        const auto world = XMMatrixTranspose(XMLoadFloat4x4(&instance.world));
        for (uint32_t i = target.firstIndex; i + 2 < target.firstIndex + target.indexCount; i += 3)
        {
            const auto a = XMVector3TransformCoord(XMLoadFloat3(&mesh.vertices[mesh.indices[i]].position), world);
            const auto b = XMVector3TransformCoord(XMLoadFloat3(&mesh.vertices[mesh.indices[i + 1]].position), world);
            const auto c = XMVector3TransformCoord(XMLoadFloat3(&mesh.vertices[mesh.indices[i + 2]].position), world);
            float distance = 0;
            if (TriangleTests::Intersects(rayOrigin, rayDirection, a, b, c, distance) && distance < closest)
            {
                closest = distance;
                result = target.nodeId;
            }
        }
    }
    return result;
}

std::optional<std::string> Tank::Rendering::MapEditorScenePresenter::PickScreen(
    float x, float y, float width, float height) const
{
    using namespace DirectX;
    if (!(width > 0 && height > 0) || x < 0 || y < 0 || x >= width || y >= height) return std::nullopt;
    const auto& camera = m_builder->GetScene().camera;
    const auto projection = Engine::CreateCameraProjectionMatrix(camera, width / height);
    const auto view = Engine::CreateCameraViewMatrix(camera);
    const auto nearPoint = XMVector3Unproject(XMVectorSet(x, y, 0, 1), 0, 0, width, height,
        0, 1, projection, view, XMMatrixIdentity());
    const auto farPoint = XMVector3Unproject(XMVectorSet(x, y, 1, 1), 0, 0, width, height,
        0, 1, projection, view, XMMatrixIdentity());
    XMFLOAT3 origin, direction;
    XMStoreFloat3(&origin, nearPoint);
    XMStoreFloat3(&direction, XMVectorSubtract(farPoint, nearPoint));
    return PickRay({origin.x, origin.y, origin.z}, {direction.x, direction.y, direction.z});
}
