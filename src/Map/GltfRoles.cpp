#include "GltfRoles.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace Tank::Map
{
    namespace
    {
        using Json = nlohmann::json;
        void Require(bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        }
        size_t Index(const Json& value, size_t count, const char* field)
        {
            Require(value.is_number_integer() && value >= 0 && value < count,
                std::string("Invalid index: ") + field);
            return value.get<size_t>();
        }
        uint32_t ReadWord(std::istream& stream)
        {
            unsigned char bytes[4];
            Require(static_cast<bool>(stream.read(reinterpret_cast<char*>(bytes), 4)), "Truncated GLB header.");
            return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
                (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
        }
        std::string ReadJson(const std::filesystem::path& path)
        {
            auto extension = path.extension().wstring();
            for (auto& c : extension) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
            Require(extension == L".gltf" || extension == L".glb", "Expected .gltf or .glb.");
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            Require(static_cast<bool>(file), "Cannot open model file.");
            const auto length = file.tellg();
            Require(length >= 0, "Cannot determine model file size.");
            file.seekg(0);
            uint32_t jsonLength = 0;
            // Metadata inspection does not allocate the potentially large BIN chunk.
            constexpr uint32_t maxJsonBytes = 16 * 1024 * 1024;
            if (extension == L".glb")
            {
                Require(ReadWord(file) == 0x46546c67, "Invalid GLB magic.");
                Require(ReadWord(file) == 2, "Only GLB version 2 is supported.");
                Require(ReadWord(file) == static_cast<uint64_t>(length), "GLB file length mismatch.");
                jsonLength = ReadWord(file);
                Require(ReadWord(file) == 0x4e4f534a, "GLB must begin with a JSON chunk.");
                Require(jsonLength % 4 == 0 && uint64_t(jsonLength) + 20 <= static_cast<uint64_t>(length),
                    "Invalid GLB JSON chunk length.");
            }
            else
            {
                Require(length <= maxJsonBytes, "glTF JSON exceeds 16 MiB inspection limit.");
                jsonLength = static_cast<uint32_t>(length);
            }
            Require(jsonLength <= maxJsonBytes, "glTF JSON exceeds 16 MiB inspection limit.");
            std::string json(jsonLength, '\0');
            Require(static_cast<bool>(file.read(json.data(), jsonLength)), "Cannot read model JSON.");
            if (extension == L".glb")
            {
                bool hasBin = false;
                uint64_t offset = uint64_t(jsonLength) + 20;
                while (offset < static_cast<uint64_t>(length))
                {
                    Require(static_cast<uint64_t>(length) - offset >= 8, "Truncated GLB chunk.");
                    const auto chunkLength = ReadWord(file);
                    const auto type = ReadWord(file);
                    Require(chunkLength % 4 == 0 && uint64_t(chunkLength) <= static_cast<uint64_t>(length) - offset - 8,
                        "Invalid GLB chunk length.");
                    Require(type != 0x4e4f534a, "Duplicate GLB JSON chunk.");
                    if (type == 0x004e4942)
                    {
                        Require(!hasBin && offset == uint64_t(jsonLength) + 20, "BIN must be the second GLB chunk.");
                        hasBin = true;
                    }
                    file.seekg(chunkLength, std::ios::cur);
                    Require(static_cast<bool>(file), "Cannot seek GLB chunk.");
                    offset += uint64_t(chunkLength) + 8;
                }
            }
            return json;
        }
    }

    bool ParseGltfRoles(const std::string& text, GltfRoles& output, std::string& error)
    {
        try
        {
            const auto json = Json::parse(text);
            Require(json.at("asset").at("version") == "2.0", "Only glTF 2.0 is supported.");
            const auto& nodes = json.at("nodes");
            const auto& meshes = json.at("meshes");
            const auto& scenes = json.at("scenes");
            Require(nodes.is_array() && meshes.is_array() && scenes.is_array() && !scenes.empty(),
                "Expected nodes, meshes and scenes arrays.");
            Require(json.contains("scene") || scenes.size() == 1,
                "Multiple scenes require an explicit default scene.");
            const size_t scene = json.contains("scene") ? Index(json["scene"], scenes.size(), "scene") : 0;
            const auto& roots = scenes.at(scene).at("nodes");
            Require(roots.is_array(), "Scene nodes must be an array.");
            std::vector<bool> visited(nodes.size());
            GltfRoles result;
            bool visual = false, hit = false;
            std::function<void(size_t, int, size_t)> visit = [&](size_t index, int role, size_t depth)
            {
                Require(depth <= 256, "Node hierarchy exceeds 256 levels.");
                Require(!visited[index], "Scene contains a cycle or a node with multiple parents.");
                visited[index] = true;
                const auto& node = nodes.at(index);
                Require(node.is_object(), "Node must be an object.");
                const auto name = node.value("name", std::string());
                if (name == "Visual" || name == "Hit")
                {
                    Require(role == -1, "Visual/Hit role parents must not be nested.");
                    Require(!node.contains("mesh"), "Visual/Hit must be mesh-free parent nodes.");
                    role = name == "Visual" ? 0 : 1;
                }
                if (node.contains("mesh"))
                {
                    Require(role != -1, "Mesh node is outside Visual/Hit: " + std::to_string(index));
                    const auto mesh = Index(node["mesh"], meshes.size(), "mesh");
                    Require(meshes[mesh].is_object(), "Mesh must be an object.");
                    result.meshNodes.push_back({ index, mesh, role == 0 ? MeshRole::Visual : MeshRole::Hit, name });
                    if (role == 0) visual = true; else hit = true;
                }
                if (node.contains("children"))
                {
                    Require(node["children"].is_array(), "Node children must be an array.");
                    for (const auto& child : node["children"])
                        visit(Index(child, nodes.size(), "child"), role, depth + 1);
                }
            };
            for (const auto& root : roots) visit(Index(root, nodes.size(), "root"), -1, 0);
            Require(visual && hit, "Both Visual and Hit must contain at least one mesh node.");
            output = std::move(result);
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool InspectGltfRoles(const std::filesystem::path& path, GltfRoles& output, std::string& error)
    {
        try { return ParseGltfRoles(ReadJson(path), output, error); }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
