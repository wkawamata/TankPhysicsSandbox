#pragma once

#include <filesystem>
#include <array>
#include <string>
#include <vector>

namespace Tank::Map
{
    struct GltfMaterialColor
    {
        std::string name;
        std::array<float, 4> baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    enum class MeshRole { Visual, Hit };
    struct RoleMeshNode
    {
        size_t nodeIndex = 0;
        size_t meshIndex = 0;
        MeshRole role = MeshRole::Visual;
        std::string name;
    };
    struct GltfRoles
    {
        std::vector<RoleMeshNode> meshNodes;
    };

    // Inspects glTF 2.0 JSON and its active scene. Does not load geometry or textures.
    // Exact Visual/Hit names designate mesh-free parent nodes. Hit is required;
    // Visual is optional so collision geometry can be used as a visible fallback.
    // Failure leaves output unchanged.
    bool InspectGltfRoles(const std::filesystem::path& path, GltfRoles& output, std::string& error);
    bool ParseGltfRoles(const std::string& json, GltfRoles& output, std::string& error);
    bool InspectGltfMaterialColors(const std::filesystem::path& path,
        std::vector<GltfMaterialColor>& output, std::string& error);
    bool SaveGltfMaterialColor(const std::filesystem::path& path, size_t materialIndex,
        const std::array<float, 4>& color, std::string& error);
}
