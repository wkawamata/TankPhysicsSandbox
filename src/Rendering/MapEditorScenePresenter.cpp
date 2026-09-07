#include "MapEditorScenePresenter.h"

#include "Map/GltfRoles.h"

#include <DirectXMath.h>

namespace
{
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string value = path.u8string();
        return { value.begin(), value.end() };
    }

    DirectX::XMMATRIX ToWorld(const Tank::Map::Transform& transform)
    {
        using namespace DirectX;
        constexpr float degreesToRadians = XM_PI / 180.0f;
        return XMMatrixRotationRollPitchYaw(
                   transform.rotationDegrees[0] * degreesToRadians,
                   transform.rotationDegrees[1] * degreesToRadians,
                   transform.rotationDegrees[2] * degreesToRadians) *
            XMMatrixTranslation(transform.position[0], transform.position[1], transform.position[2]);
    }
}

bool Tank::Rendering::MapEditorScenePresenter::Rebuild(
    const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
    const MapEditorGridSettings& grid, std::string& error)
{
    auto next = std::make_unique<Engine::SceneBuilder>();
    const uint32_t fallbackMaterial = next->AddSolidColorMaterial(230, 230, 230, 255);
    const uint32_t gridMaterial = next->AddSolidColorMaterial(100, 150, 185, 255);
    const uint32_t xAxisMaterial = next->AddSolidColorMaterial(210, 85, 85, 255);
    const uint32_t zAxisMaterial = next->AddSolidColorMaterial(85, 190, 105, 255);
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
    for (const Map::Instance& instance : manifest.instances)
    {
        const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
        const std::filesystem::path assetPath = mapFolder / std::filesystem::path(assetUtf8);
        Map::GltfRoles roles;
        std::string loadError;
        if (!Map::InspectGltfRoles(assetPath, roles, loadError))
        {
            error = "Cannot read '" + instance.asset + "': " + loadError;
            return false;
        }
        const Engine::GltfSceneAssetLoadResult asset = Engine::LoadGltfSceneAsset(ToUtf8(assetPath));
        if (!asset)
        {
            error = "Cannot load '" + instance.asset + "': " + asset.message;
            return false;
        }
        for (const Map::RoleMeshNode& node : roles.meshNodes)
        {
            if (node.role != Map::MeshRole::Visual)
                continue;
            const Engine::GltfNodeMeshAddResult add = next->AddGltfNodeMesh(asset.asset, static_cast<uint32_t>(node.nodeIndex));
            if (!add)
            {
                error = "Cannot load Visual node from '" + instance.asset + "': " + add.message;
                return false;
            }
            next->AddInstance(*add.meshId, ToWorld(instance.transform), fallbackMaterial);
        }
    }
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
