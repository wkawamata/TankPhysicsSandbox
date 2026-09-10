#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Tank::Map
{
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
    // Exact Visual/Hit names designate mesh-free parent nodes. Both roles required.
    // Failure leaves output unchanged.
    bool InspectGltfRoles(const std::filesystem::path& path, GltfRoles& output, std::string& error);
    bool ParseGltfRoles(const std::string& json, GltfRoles& output, std::string& error);
}
