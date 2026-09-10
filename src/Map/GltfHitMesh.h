#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "MapManifest.h"

namespace Tank::Map
{
    struct HitTriangleMesh
    {
        std::vector<std::array<float, 3>> vertices;
        std::vector<std::array<uint32_t, 3>> triangles;
    };

    // Loads Hit-role triangle primitives and bakes their glTF hierarchy transforms.
    // Output uses the map's left-handed, Y-up, meter convention. Failure leaves output unchanged.
    bool LoadGltfHitMesh(const std::filesystem::path& path, HitTriangleMesh& output, std::string& error);

    // Applies a Manifest placement using the same roll-pitch-yaw matrix as the renderer.
    bool TransformHitMesh(const HitTriangleMesh& input, const Transform& transform,
        HitTriangleMesh& output, std::string& error);
}
