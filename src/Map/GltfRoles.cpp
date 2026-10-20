#include "GltfRoles.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
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

    static bool ParseMaterialColors(const std::string& text, std::vector<GltfMaterialColor>& output,
        std::string& error)
    {
        try
        {
            const auto json = Json::parse(text);
            Require(json.value("asset", Json::object()).value("version", std::string()) == "2.0",
                "Only glTF 2.0 is supported.");
            std::vector<GltfMaterialColor> result;
            if (json.contains("materials"))
            {
                Require(json["materials"].is_array(), "Materials must be an array.");
                for (const auto& material : json["materials"])
                {
                    Require(material.is_object(), "Material must be an object.");
                    GltfMaterialColor item;
                    item.name = material.value("name", std::string());
                    if (material.contains("pbrMetallicRoughness") &&
                        material["pbrMetallicRoughness"].contains("baseColorFactor"))
                    {
                        const auto& factor = material["pbrMetallicRoughness"]["baseColorFactor"];
                        Require(factor.is_array() && factor.size() == 4,
                            "baseColorFactor must have four values.");
                        for (size_t channel = 0; channel < 4; ++channel)
                        {
                            Require(factor[channel].is_number(), "baseColorFactor values must be numbers.");
                            item.baseColorFactor[channel] = factor[channel].get<float>();
                            Require(std::isfinite(item.baseColorFactor[channel]) &&
                                item.baseColorFactor[channel] >= 0.0f && item.baseColorFactor[channel] <= 1.0f,
                                "baseColorFactor values must be between 0 and 1.");
                        }
                    }
                    result.push_back(std::move(item));
                }
            }
            output = std::move(result);
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    static bool WriteJson(const std::filesystem::path& path, const std::string& text, std::string& error)
    {
        try
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            const auto json = Json::parse(text);
            const std::string formatted = json.dump(2) + "\n";
            if (extension == ".gltf")
            {
                std::ofstream file(path, std::ios::binary | std::ios::trunc);
                Require(static_cast<bool>(file), "Cannot open glTF for writing.");
                file.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
                Require(static_cast<bool>(file), "Cannot write glTF.");
                error.clear();
                return true;
            }
            Require(extension == ".glb", "Expected .gltf or .glb.");
            std::ifstream input(path, std::ios::binary);
            Require(static_cast<bool>(input), "Cannot open GLB for reading.");
            std::vector<char> original((std::istreambuf_iterator<char>(input)), {});
            Require(original.size() >= 20 && original[0] == 'g' && original[1] == 'l' &&
                original[2] == 'T' && original[3] == 'F', "Invalid GLB header.");
            const auto readWord = [&original](size_t offset)
            {
                return uint32_t(static_cast<unsigned char>(original[offset])) |
                    (uint32_t(static_cast<unsigned char>(original[offset + 1])) << 8) |
                    (uint32_t(static_cast<unsigned char>(original[offset + 2])) << 16) |
                    (uint32_t(static_cast<unsigned char>(original[offset + 3])) << 24);
            };
            Require(readWord(4) == 2 && readWord(8) == original.size() && readWord(16) == 0x4e4f534a,
                "Invalid GLB 2.0 JSON chunk.");
            const size_t oldJsonLength = readWord(12);
            Require(oldJsonLength % 4 == 0 && oldJsonLength <= original.size() - 20,
                "Invalid GLB JSON chunk length.");
            std::string padded = formatted;
            while (padded.size() % 4) padded.push_back(' ');
            const size_t tailOffset = 20 + oldJsonLength;
            std::vector<char> updated;
            updated.reserve(original.size() - oldJsonLength + padded.size());
            updated.insert(updated.end(), original.begin(), original.begin() + 12);
            const uint32_t jsonLength = static_cast<uint32_t>(padded.size());
            for (int byte = 0; byte < 4; ++byte) updated.push_back(static_cast<char>((jsonLength >> (byte * 8)) & 255));
            updated.insert(updated.end(), original.begin() + 16, original.begin() + 20);
            updated.insert(updated.end(), padded.begin(), padded.end());
            updated.insert(updated.end(), original.begin() + tailOffset, original.end());
            const uint32_t totalLength = static_cast<uint32_t>(updated.size());
            for (int byte = 0; byte < 4; ++byte) updated[8 + byte] = static_cast<char>((totalLength >> (byte * 8)) & 255);
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            Require(static_cast<bool>(file), "Cannot open GLB for writing.");
            file.write(updated.data(), static_cast<std::streamsize>(updated.size()));
            Require(static_cast<bool>(file), "Cannot write GLB.");
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
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
            bool hit = false;
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
                    if (role == 1) hit = true;
                }
                if (node.contains("children"))
                {
                    Require(node["children"].is_array(), "Node children must be an array.");
                    for (const auto& child : node["children"])
                        visit(Index(child, nodes.size(), "child"), role, depth + 1);
                }
            };
            for (const auto& root : roots) visit(Index(root, nodes.size(), "root"), -1, 0);
            Require(hit, "Hit must contain at least one mesh node.");
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

    bool InspectGltfMaterialColors(const std::filesystem::path& path,
        std::vector<GltfMaterialColor>& output, std::string& error)
    {
        try { return ParseMaterialColors(ReadJson(path), output, error); }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool SaveGltfMaterialColor(const std::filesystem::path& path, size_t materialIndex,
        const std::array<float, 4>& color, std::string& error)
    {
        try
        {
            for (float channel : color)
                Require(std::isfinite(channel) && channel >= 0.0f && channel <= 1.0f,
                    "Color channels must be between 0 and 1.");
            Json json = Json::parse(ReadJson(path));
            Require(json.contains("materials") && json["materials"].is_array() &&
                materialIndex < json["materials"].size(), "Material index is out of range.");
            auto& material = json["materials"][materialIndex];
            if (!material.contains("pbrMetallicRoughness"))
                material["pbrMetallicRoughness"] = Json::object();
            material["pbrMetallicRoughness"]["baseColorFactor"] =
                Json::array({ color[0], color[1], color[2], color[3] });
            return WriteJson(path, json.dump(), error);
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
