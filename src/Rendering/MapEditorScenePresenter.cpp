#include "MapEditorScenePresenter.h"

#include "Map/GltfRoles.h"
#include "MapVisualLoader.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

namespace
{
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string value = path.u8string();
        return { value.begin(), value.end() };
    }

    DirectX::XMMATRIX ToWorld(const Tank::Map::Transform& transform)
    {
        constexpr float degreesToRadians = DirectX::XM_PI / 180.0f;
        return DirectX::XMMatrixRotationRollPitchYaw(
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

}

bool Tank::Rendering::MapEditorScenePresenter::Rebuild(
    const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
    const MapEditorGridSettings& grid, std::string& error)
{
    return Rebuild(mapFolder, manifest, grid, {}, error);
}

bool Tank::Rendering::MapEditorScenePresenter::Rebuild(
    const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
    const MapEditorGridSettings& grid, const MapEditorPreviewSettings& preview,
    std::string& error)
{
    auto next = std::make_unique<Engine::SceneBuilder>();
    const uint32_t fallbackMaterial = next->AddSolidColorMaterial(230, 230, 230, 255);
    const uint32_t gridMaterial = next->AddSolidColorMaterial(100, 150, 185, 255);
    const uint32_t xAxisMaterial = next->AddSolidColorMaterial(210, 85, 85, 255);
    const uint32_t zAxisMaterial = next->AddSolidColorMaterial(85, 190, 105, 255);
    const uint32_t spawnMaterial = next->AddSolidColorMaterial(255, 190, 35, 255);
    const uint32_t clearAreaMaterial = next->AddSolidColorMaterial(35, 225, 235, 255);
    const uint32_t selectionMaterial = next->AddSolidColorMaterial(255, 220, 35, 255);
    const Engine::SceneMeshId gridMesh = next->AddCube(1.0f);
    const float extent = grid.spacingMeters * static_cast<float>(grid.halfCellCount);
    for (int cell = -grid.halfCellCount; cell <= grid.halfCellCount; ++cell)
    {
        const float offset = grid.spacingMeters * static_cast<float>(cell);
        const uint32_t xAxisLineMaterial = cell == 0 ? xAxisMaterial : gridMaterial;
        const uint32_t zAxisLineMaterial = cell == 0 ? zAxisMaterial : gridMaterial;
        next->AddInstance(gridMesh,
            DirectX::XMMatrixScaling(extent * 2.0f, grid.lineWidthMeters, grid.lineWidthMeters) *
                DirectX::XMMatrixTranslation(0.0f, -grid.lineWidthMeters * 0.5f, offset), zAxisLineMaterial);
        next->AddInstance(gridMesh,
            DirectX::XMMatrixScaling(grid.lineWidthMeters, grid.lineWidthMeters, extent * 2.0f) *
                DirectX::XMMatrixTranslation(offset, -grid.lineWidthMeters * 0.5f, 0.0f), xAxisLineMaterial);
    }
    Map::Manifest visibleManifest = manifest;
    std::erase_if(visibleManifest.instances, [&preview](const Map::Instance& instance)
        { return preview.hiddenInstanceIds.contains(instance.id); });
    std::vector<MapVisualBounds> visualBounds;
    if (!AppendMapVisuals(*next, mapFolder, visibleManifest, fallbackMaterial, error, &visualBounds))
        return false;
    const auto selectedInstance = std::find_if(visibleManifest.instances.begin(), visibleManifest.instances.end(),
        [&preview](const Map::Instance& instance) { return instance.id == preview.selectedInstanceId; });
    const auto selectedBounds = std::find_if(visualBounds.begin(), visualBounds.end(),
        [&preview](const MapVisualBounds& bounds) { return bounds.instanceId == preview.selectedInstanceId; });
    if (selectedInstance != visibleManifest.instances.end() && selectedBounds != visualBounds.end())
        AppendSelectionWireframe(*next, gridMesh, *selectedInstance, *selectedBounds, selectionMaterial);
    std::vector<size_t> markerInstances;
    AppendMapMarkers(*next, gridMesh, manifest, spawnMaterial,
        clearAreaMaterial, markerInstances);
    next->GetScene().camera.pos = { 8.0f, 6.0f, -8.0f };
    next->GetScene().camera.gazePoint = { 0.0f, 0.0f, 0.0f };
    m_builder = std::move(next);
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
    for (const Map::RoleMeshNode& node : roles.meshNodes)
    {
        if (node.role != Map::MeshRole::Visual)
            continue;
        const Engine::GltfNodeMeshAddResult add = builder.AddGltfNodeMesh(asset.asset, static_cast<uint32_t>(node.nodeIndex));
        if (!add)
        {
            error = add.message;
            return false;
        }
    }
    error.clear();
    return true;
}

void Tank::Rendering::MapEditorScenePresenter::Clear()
{
    m_builder = std::make_unique<Engine::SceneBuilder>();
}
