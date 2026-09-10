#include "Map/GltfHitMesh.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    bool Near(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    std::vector<uint8_t> ReadBytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        Check(static_cast<bool>(file), "Open test fixture");
        const auto length = file.tellg();
        file.seekg(0);
        std::vector<uint8_t> bytes(static_cast<size_t>(length));
        file.read(reinterpret_cast<char*>(bytes.data()), length);
        Check(static_cast<bool>(file), "Read test fixture");
        return bytes;
    }

    std::string Base64(const std::vector<uint8_t>& bytes)
    {
        static constexpr char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        for (size_t offset = 0; offset < bytes.size(); offset += 3)
        {
            const size_t remaining = bytes.size() - offset;
            const uint32_t value = uint32_t(bytes[offset]) << 16 |
                (remaining > 1 ? uint32_t(bytes[offset + 1]) << 8 : 0) |
                (remaining > 2 ? uint32_t(bytes[offset + 2]) : 0);
            result.push_back(alphabet[(value >> 18) & 63]);
            result.push_back(alphabet[(value >> 12) & 63]);
            result.push_back(remaining > 1 ? alphabet[(value >> 6) & 63] : '=');
            result.push_back(remaining > 2 ? alphabet[value & 63] : '=');
        }
        return result;
    }

    std::string Replace(std::string value, const std::string& from, const std::string& to)
    {
        const size_t position = value.find(from);
        Check(position != std::string::npos, "Find fixture text to replace");
        value.replace(position, from.size(), to);
        return value;
    }

    void WriteWord(std::ofstream& file, uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
            file.put(static_cast<char>((value >> (byte * 8)) & 255));
    }

    struct TemporaryDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "TankGltfHitMeshTests";
        TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
            Check(std::filesystem::create_directory(path), "Create temporary folder");
        }
        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}

int main()
{
    try
    {
        const std::filesystem::path fixture =
            std::filesystem::path(__FILE__).parent_path() / "Fixtures" / "MapEditor" / "triangle-visual-hit.gltf";
        Tank::Map::HitTriangleMesh mesh;
        std::string error;
        Check(Tank::Map::LoadGltfHitMesh(fixture, mesh, error), "Load Hit triangle from external BIN glTF");
        Check(mesh.vertices.size() == 3 && mesh.triangles.size() == 1, "Extract only the Hit mesh geometry");
        Check(Near(mesh.vertices[0][0], 1.0f) && Near(mesh.vertices[0][1], 2.0f) &&
            Near(mesh.vertices[0][2], -3.0f) && Near(mesh.vertices[1][0], 1.0f) &&
            Near(mesh.vertices[1][2], -1.0f) && Near(mesh.vertices[2][0], 3.0f) &&
            Near(mesh.vertices[2][2], -3.0f),
            "Bake Hit parent and child transforms before converting to map left-handed positions");
        Check(mesh.triangles[0] == std::array<uint32_t, 3> { 0, 1, 2 },
            "Reverse glTF winding while preserving the fixture front face");

        Tank::Map::Transform transform;
        transform.position = { 10.0f, 20.0f, 30.0f };
        transform.rotationDegrees = { 0.0f, 90.0f, 0.0f };
        Tank::Map::HitTriangleMesh placed;
        Check(Tank::Map::TransformHitMesh(mesh, transform, placed, error),
            "Apply Manifest placement to HitMesh");
        Check(Near(placed.vertices[0][0], 7.0f) && Near(placed.vertices[0][1], 22.0f) &&
            Near(placed.vertices[0][2], 29.0f),
            "Manifest yaw and translation match the renderer roll-pitch-yaw matrix");

        const std::vector<uint8_t> binary = ReadBytes(fixture.parent_path() / "triangle-visual-hit.bin");
        const std::vector<uint8_t> jsonBytes = ReadBytes(fixture);
        const std::string json(jsonBytes.begin(), jsonBytes.end());
        TemporaryDirectory temporary;
        const auto embeddedPath = temporary.path / "embedded.gltf";
        const std::string embedded = Replace(json, "triangle-visual-hit.bin",
            "data:application/octet-stream;base64," + Base64(binary));
        { std::ofstream file(embeddedPath, std::ios::binary); file << embedded; }
        Check(Tank::Map::LoadGltfHitMesh(embeddedPath, mesh, error) && mesh.triangles.size() == 1,
            "Load base64 embedded glTF buffer");

        std::string glbJson = Replace(json, ",\"uri\":\"triangle-visual-hit.bin\"", "");
        while (glbJson.size() % 4) glbJson.push_back(' ');
        std::vector<uint8_t> paddedBinary = binary;
        while (paddedBinary.size() % 4) paddedBinary.push_back(0);
        const auto glbPath = temporary.path / "embedded.glb";
        {
            std::ofstream file(glbPath, std::ios::binary);
            WriteWord(file, 0x46546c67); WriteWord(file, 2);
            WriteWord(file, static_cast<uint32_t>(12 + 8 + glbJson.size() + 8 + paddedBinary.size()));
            WriteWord(file, static_cast<uint32_t>(glbJson.size())); WriteWord(file, 0x4e4f534a);
            file.write(glbJson.data(), static_cast<std::streamsize>(glbJson.size()));
            WriteWord(file, static_cast<uint32_t>(paddedBinary.size())); WriteWord(file, 0x004e4942);
            file.write(reinterpret_cast<const char*>(paddedBinary.data()),
                static_cast<std::streamsize>(paddedBinary.size()));
        }
        Check(Tank::Map::LoadGltfHitMesh(glbPath, mesh, error) && mesh.triangles.size() == 1,
            "Load GLB binary chunk");

        Tank::Map::HitTriangleMesh unchanged = mesh;
        Check(!Tank::Map::LoadGltfHitMesh(fixture.parent_path() / "missing.gltf", unchanged, error) &&
            unchanged.vertices.size() == 3 && unchanged.triangles.size() == 1,
            "Failure leaves the previous collision mesh unchanged");
        std::cout << "glTF HitMesh tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
