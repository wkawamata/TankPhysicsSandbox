#pragma once

#include "Map/MapManifest.h"
#include "Map/GltfHitMesh.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine { class SceneBuilder; using SceneMeshId = uint32_t; }

namespace Tank::Rendering
{
    inline constexpr size_t kPlayerStartMarkerPartCount = 3u;
    inline constexpr size_t kClearAreaMarkerPartCount = 12u;

    bool AppendMapVisuals(Engine::SceneBuilder& builder,
        const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
        uint32_t materialId, std::string& error);

    bool AppendMapHitMeshOverlay(Engine::SceneBuilder& builder,
        const std::vector<Map::HitTriangleMesh>& hitMeshes, uint32_t materialId,
        size_t& instanceIndex, std::string& error);

    void AppendMapMarkers(Engine::SceneBuilder& builder, Engine::SceneMeshId cubeMesh,
        const Map::Manifest& manifest, uint32_t spawnMaterialId,
        uint32_t clearAreaMaterialId, std::vector<size_t>& instanceIndices);

    void AppendMapClearBeacons(Engine::SceneBuilder& builder, Engine::SceneMeshId cubeMesh,
        const Map::Manifest& manifest, uint32_t materialId,
        std::vector<size_t>& instanceIndices);
}
