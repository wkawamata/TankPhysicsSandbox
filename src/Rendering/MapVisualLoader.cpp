#include "MapVisualLoader.h"

#include "Map/GltfRoles.h"
#include "Scene/SceneBuilder.h"

#include <DirectXMath.h>
#include <GltfLoader.h>

#include <limits>
#include <vector>

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

    void AddMarkerInstance(Engine::SceneBuilder& builder, Engine::SceneMeshId cubeMesh,
        DirectX::FXMMATRIX world, uint32_t materialId, std::vector<size_t>& instanceIndices)
    {
        instanceIndices.push_back(builder.GetScene().instances.size());
        builder.AddInstance(cubeMesh, world, materialId);
    }

    void AddClearAreaWireframe(Engine::SceneBuilder& builder, Engine::SceneMeshId cubeMesh,
        uint32_t material, const Tank::Map::ClearArea& area,
        std::vector<size_t>& instanceIndices)
    {
        const float x = area.size[0];
        const float y = area.size[1];
        const float z = area.size[2];
        const float halfX = x * 0.5f;
        const float halfY = y * 0.5f;
        const float halfZ = z * 0.5f;
        const float thickness = 0.035f;
        const auto addEdge = [&](float sx, float sy, float sz, float px, float py, float pz)
        {
            AddMarkerInstance(builder, cubeMesh,
                DirectX::XMMatrixScaling(sx, sy, sz) * DirectX::XMMatrixTranslation(
                    area.center[0] + px, area.center[1] + py, area.center[2] + pz),
                material, instanceIndices);
        };
        for (const float oy : { -halfY, halfY })
        {
            for (const float oz : { -halfZ, halfZ })
                addEdge(x, thickness, thickness, 0.0f, oy, oz);
        }
        for (const float ox : { -halfX, halfX })
        {
            for (const float oz : { -halfZ, halfZ })
                addEdge(thickness, y, thickness, ox, 0.0f, oz);
        }
        for (const float ox : { -halfX, halfX })
        {
            for (const float oy : { -halfY, halfY })
                addEdge(thickness, thickness, z, ox, oy, 0.0f);
        }
    }
}

bool Tank::Rendering::AppendMapVisuals(Engine::SceneBuilder& builder,
    const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
    uint32_t materialId, std::string& error)
{
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
            if (node.role != Map::MeshRole::Visual) continue;
            const Engine::GltfNodeMeshAddResult add =
                builder.AddGltfNodeMesh(asset.asset, static_cast<uint32_t>(node.nodeIndex));
            if (!add)
            {
                error = "Cannot load Visual node from '" + instance.asset + "': " + add.message;
                return false;
            }
            builder.AddInstance(*add.meshId, ToWorld(instance.transform), materialId);
        }
    }
    error.clear();
    return true;
}

bool Tank::Rendering::AppendMapHitMeshOverlay(Engine::SceneBuilder& builder,
    const std::vector<Map::HitTriangleMesh>& hitMeshes, uint32_t materialId,
    size_t& instanceIndex, std::string& error)
{
    std::vector<Engine::SceneVertex> vertices;
    std::vector<uint32_t> indices;
    for (const Map::HitTriangleMesh& hitMesh : hitMeshes)
    {
        for (const std::array<uint32_t, 3>& triangle : hitMesh.triangles)
        {
            if (triangle[0] >= hitMesh.vertices.size() ||
                triangle[1] >= hitMesh.vertices.size() ||
                triangle[2] >= hitMesh.vertices.size())
            {
                error = "HitMesh overlay contains an out-of-range triangle index.";
                return false;
            }
            if (vertices.size() > std::numeric_limits<uint32_t>::max() - 3u)
            {
                error = "HitMesh overlay is too large.";
                return false;
            }

            const std::array<float, 3>& a = hitMesh.vertices[triangle[0]];
            const std::array<float, 3>& b = hitMesh.vertices[triangle[1]];
            const std::array<float, 3>& c = hitMesh.vertices[triangle[2]];
            const DirectX::XMVECTOR av = DirectX::XMVectorSet(a[0], a[1], a[2], 0.0f);
            const DirectX::XMVECTOR bv = DirectX::XMVectorSet(b[0], b[1], b[2], 0.0f);
            const DirectX::XMVECTOR cv = DirectX::XMVectorSet(c[0], c[1], c[2], 0.0f);
            const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(
                DirectX::XMVectorSubtract(bv, av), DirectX::XMVectorSubtract(cv, av));
            const float lengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(cross));
            if (!(lengthSquared > 1.0e-12f))
            {
                error = "HitMesh overlay contains a degenerate triangle.";
                return false;
            }
            DirectX::XMFLOAT3 normal;
            DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(cross));
            const uint32_t first = static_cast<uint32_t>(vertices.size());
            for (const std::array<float, 3>* point : { &a, &b, &c })
            {
                vertices.push_back({
                    { (*point)[0], (*point)[1], (*point)[2] },
                    { 0.0f, 0.0f },
                    normal });
            }
            indices.insert(indices.end(), { first, first + 1u, first + 2u });
        }
    }
    if (indices.empty())
    {
        error = "HitMesh overlay contains no triangles.";
        return false;
    }

    Engine::SceneMesh& sceneMesh = builder.GetMesh();
    if (sceneMesh.vertices.size() > std::numeric_limits<uint32_t>::max() - vertices.size() ||
        sceneMesh.indices.size() > std::numeric_limits<uint32_t>::max() - indices.size())
    {
        error = "Scene mesh is too large for a HitMesh overlay.";
        return false;
    }
    const uint32_t firstVertex = static_cast<uint32_t>(sceneMesh.vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(sceneMesh.indices.size());
    sceneMesh.vertices.insert(sceneMesh.vertices.end(), vertices.begin(), vertices.end());
    for (const uint32_t index : indices)
    {
        sceneMesh.indices.push_back(firstVertex + index);
    }
    sceneMesh.ranges.push_back({ firstVertex, static_cast<uint32_t>(vertices.size()),
        firstIndex, static_cast<uint32_t>(indices.size()) });
    const Engine::SceneMeshId meshId = static_cast<Engine::SceneMeshId>(sceneMesh.ranges.size() - 1u);
    instanceIndex = builder.GetScene().instances.size();
    builder.AddInstance(meshId, DirectX::XMMatrixScaling(0.0f, 0.0f, 0.0f), materialId);
    error.clear();
    return true;
}

void Tank::Rendering::AppendMapMarkers(Engine::SceneBuilder& builder,
    Engine::SceneMeshId cubeMesh, const Map::Manifest& manifest,
    uint32_t spawnMaterialId, uint32_t clearAreaMaterialId,
    std::vector<size_t>& instanceIndices)
{
    std::vector<size_t> appended;
    appended.reserve(3u + manifest.clearAreas.size() * 12u);
    const DirectX::XMMATRIX spawnWorld = ToWorld(manifest.playerSpawn);
    AddMarkerInstance(builder, cubeMesh,
        DirectX::XMMatrixScaling(0.10f, 1.5f, 0.10f) *
            DirectX::XMMatrixTranslation(0.0f, 0.75f, 0.0f) * spawnWorld,
        spawnMaterialId, appended);
    AddMarkerInstance(builder, cubeMesh,
        DirectX::XMMatrixScaling(0.14f, 0.14f, 1.4f) *
            DirectX::XMMatrixTranslation(0.0f, 1.5f, 0.7f) * spawnWorld,
        spawnMaterialId, appended);
    AddMarkerInstance(builder, cubeMesh,
        DirectX::XMMatrixScaling(0.5f, 0.22f, 0.5f) *
            DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4) *
            DirectX::XMMatrixTranslation(0.0f, 1.5f, 1.4f) * spawnWorld,
        spawnMaterialId, appended);
    for (const Map::ClearArea& area : manifest.clearAreas)
        AddClearAreaWireframe(builder, cubeMesh, clearAreaMaterialId, area, appended);
    instanceIndices = std::move(appended);
}

void Tank::Rendering::AppendMapClearBeacons(Engine::SceneBuilder& builder,
    Engine::SceneMeshId cubeMesh, const Map::Manifest& manifest,
    uint32_t materialId, std::vector<size_t>& instanceIndices)
{
    constexpr float beaconWidth = 0.35f;
    constexpr float beaconHeight = 12.0f;
    std::vector<size_t> appended;
    appended.reserve(manifest.clearAreas.size());
    for (const Map::ClearArea& area : manifest.clearAreas)
    {
        AddMarkerInstance(builder, cubeMesh,
            DirectX::XMMatrixScaling(beaconWidth, beaconHeight, beaconWidth) *
                DirectX::XMMatrixTranslation(
                    area.center[0], area.center[1] + beaconHeight * 0.5f, area.center[2]),
            materialId, appended);
    }
    instanceIndices = std::move(appended);
}
