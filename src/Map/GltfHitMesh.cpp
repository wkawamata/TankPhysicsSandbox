#include "GltfHitMesh.h"

#include "GltfRoles.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <unordered_set>

namespace Tank::Map
{
    namespace
    {
        using Json = nlohmann::json;
        using Matrix = std::array<std::array<double, 4>, 4>;

        void Require(bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        }

        std::vector<uint8_t> ReadBytes(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            Require(static_cast<bool>(file), "Cannot open buffer file.");
            const std::streamoff length = file.tellg();
            Require(length >= 0, "Cannot determine buffer file size.");
            file.seekg(0);
            std::vector<uint8_t> bytes(static_cast<size_t>(length));
            if (!bytes.empty())
                Require(static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), length)),
                    "Cannot read buffer file.");
            return bytes;
        }

        uint32_t Word(const std::vector<uint8_t>& bytes, size_t offset)
        {
            Require(offset + 4 <= bytes.size(), "Truncated GLB word.");
            return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
                (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
        }

        struct Document
        {
            Json json;
            std::vector<uint8_t> binaryChunk;
        };

        Document LoadDocument(const std::filesystem::path& path)
        {
            const std::vector<uint8_t> bytes = ReadBytes(path);
            std::wstring extension = path.extension().wstring();
            for (wchar_t& value : extension)
                if (value >= L'A' && value <= L'Z') value += L'a' - L'A';
            Require(extension == L".gltf" || extension == L".glb", "Expected .gltf or .glb.");
            Document document;
            if (extension == L".gltf")
            {
                document.json = Json::parse(bytes.begin(), bytes.end());
                return document;
            }

            Require(bytes.size() >= 20 && Word(bytes, 0) == 0x46546c67 && Word(bytes, 4) == 2,
                "Invalid GLB header.");
            Require(Word(bytes, 8) == bytes.size(), "GLB file length mismatch.");
            const uint32_t jsonLength = Word(bytes, 12);
            Require(Word(bytes, 16) == 0x4e4f534a && size_t(jsonLength) + 20 <= bytes.size(),
                "GLB must begin with a valid JSON chunk.");
            document.json = Json::parse(bytes.begin() + 20, bytes.begin() + 20 + jsonLength);
            size_t offset = 20 + jsonLength;
            if (offset < bytes.size())
            {
                Require(offset + 8 <= bytes.size(), "Truncated GLB chunk.");
                const uint32_t binaryLength = Word(bytes, offset);
                Require(Word(bytes, offset + 4) == 0x004e4942 && offset + 8 + binaryLength <= bytes.size(),
                    "Invalid GLB binary chunk.");
                document.binaryChunk.assign(bytes.begin() + offset + 8, bytes.begin() + offset + 8 + binaryLength);
                Require(offset + 8 + binaryLength == bytes.size(), "Unexpected GLB trailing chunk.");
            }
            return document;
        }

        std::vector<uint8_t> DecodeBase64(const std::string& encoded)
        {
            static constexpr unsigned char invalid = 0xff;
            std::array<unsigned char, 256> values;
            values.fill(invalid);
            const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for (size_t index = 0; index < alphabet.size(); ++index)
                values[static_cast<unsigned char>(alphabet[index])] = static_cast<unsigned char>(index);
            Require(encoded.size() % 4 == 0, "Invalid base64 buffer length.");
            std::vector<uint8_t> result;
            result.reserve(encoded.size() / 4 * 3);
            for (size_t index = 0; index < encoded.size(); index += 4)
            {
                uint32_t value = 0;
                int padding = 0;
                for (int digit = 0; digit < 4; ++digit)
                {
                    const unsigned char character = static_cast<unsigned char>(encoded[index + digit]);
                    if (character == '=')
                    {
                        Require(digit >= 2, "Invalid base64 buffer padding.");
                        ++padding;
                        value <<= 6;
                    }
                    else
                    {
                        Require(padding == 0 && values[character] != invalid, "Invalid base64 buffer data.");
                        value = (value << 6) | values[character];
                    }
                }
                result.push_back(static_cast<uint8_t>(value >> 16));
                if (padding < 2) result.push_back(static_cast<uint8_t>(value >> 8));
                if (padding < 1) result.push_back(static_cast<uint8_t>(value));
            }
            return result;
        }

        std::string DecodeUriPath(const std::string& uri)
        {
            std::string result;
            for (size_t index = 0; index < uri.size(); ++index)
            {
                if (uri[index] != '%')
                {
                    result.push_back(uri[index]);
                    continue;
                }
                Require(index + 2 < uri.size(), "Invalid percent escape in buffer URI.");
                const auto hex = [](char value) -> int
                {
                    if (value >= '0' && value <= '9') return value - '0';
                    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
                    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
                    return -1;
                };
                const int high = hex(uri[index + 1]);
                const int low = hex(uri[index + 2]);
                Require(high >= 0 && low >= 0, "Invalid percent escape in buffer URI.");
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2;
            }
            return result;
        }

        std::vector<std::vector<uint8_t>> LoadBuffers(
            const std::filesystem::path& modelPath, const Document& document)
        {
            const Json& descriptions = document.json.at("buffers");
            Require(descriptions.is_array(), "glTF buffers must be an array.");
            std::vector<std::vector<uint8_t>> buffers;
            for (size_t index = 0; index < descriptions.size(); ++index)
            {
                const Json& description = descriptions[index];
                std::vector<uint8_t> data;
                if (!description.contains("uri"))
                {
                    Require(index == 0 && !document.binaryChunk.empty(), "glTF buffer has no URI or GLB data.");
                    data = document.binaryChunk;
                }
                else
                {
                    const std::string uri = description.at("uri").get<std::string>();
                    const std::string prefix = "data:application/octet-stream;base64,";
                    if (uri.starts_with(prefix))
                        data = DecodeBase64(uri.substr(prefix.size()));
                    else
                    {
                        const std::string decoded = DecodeUriPath(uri);
                        const std::u8string utf8(decoded.begin(), decoded.end());
                        const std::filesystem::path relative(utf8);
                        Require(!relative.is_absolute(), "Absolute buffer URI is not allowed.");
                        for (const auto& part : relative)
                            Require(part != "..", "Parent traversal in buffer URI is not allowed.");
                        data = ReadBytes(modelPath.parent_path() / relative);
                    }
                }
                const size_t declaredLength = description.at("byteLength").get<size_t>();
                Require(data.size() >= declaredLength, "glTF buffer is shorter than byteLength.");
                buffers.push_back(std::move(data));
            }
            return buffers;
        }

        Matrix Identity()
        {
            Matrix result = {};
            for (size_t index = 0; index < 4; ++index) result[index][index] = 1.0;
            return result;
        }

        Matrix Multiply(const Matrix& left, const Matrix& right)
        {
            Matrix result = {};
            for (size_t row = 0; row < 4; ++row)
                for (size_t column = 0; column < 4; ++column)
                    for (size_t inner = 0; inner < 4; ++inner)
                        result[row][column] += left[row][inner] * right[inner][column];
            return result;
        }

        Matrix NodeTransform(const Json& node)
        {
            if (node.contains("matrix"))
            {
                const Json& values = node.at("matrix");
                Require(values.is_array() && values.size() == 16, "Node matrix must contain 16 values.");
                Matrix result = {};
                for (size_t row = 0; row < 4; ++row)
                    for (size_t column = 0; column < 4; ++column)
                        result[row][column] = values[column * 4 + row].get<double>();
                return result;
            }

            std::array<double, 3> scale = { 1.0, 1.0, 1.0 };
            std::array<double, 3> translation = {};
            std::array<double, 4> rotation = { 0.0, 0.0, 0.0, 1.0 };
            if (node.contains("scale")) scale = node.at("scale").get<std::array<double, 3>>();
            if (node.contains("translation")) translation = node.at("translation").get<std::array<double, 3>>();
            if (node.contains("rotation")) rotation = node.at("rotation").get<std::array<double, 4>>();
            const double x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
            const double length = std::sqrt(x * x + y * y + z * z + w * w);
            Require(std::isfinite(length) && length > 0.0, "Node rotation must be a finite quaternion.");
            const double qx = x / length, qy = y / length, qz = z / length, qw = w / length;
            Matrix result = Identity();
            result[0][0] = (1.0 - 2.0 * (qy * qy + qz * qz)) * scale[0];
            result[1][0] = (2.0 * (qx * qy + qz * qw)) * scale[0];
            result[2][0] = (2.0 * (qx * qz - qy * qw)) * scale[0];
            result[0][1] = (2.0 * (qx * qy - qz * qw)) * scale[1];
            result[1][1] = (1.0 - 2.0 * (qx * qx + qz * qz)) * scale[1];
            result[2][1] = (2.0 * (qy * qz + qx * qw)) * scale[1];
            result[0][2] = (2.0 * (qx * qz + qy * qw)) * scale[2];
            result[1][2] = (2.0 * (qy * qz - qx * qw)) * scale[2];
            result[2][2] = (1.0 - 2.0 * (qx * qx + qy * qy)) * scale[2];
            result[0][3] = translation[0];
            result[1][3] = translation[1];
            result[2][3] = translation[2];
            return result;
        }

        std::array<float, 3> TransformPosition(const Matrix& transform, float x, float y, float z)
        {
            const double px = transform[0][0] * x + transform[0][1] * y + transform[0][2] * z + transform[0][3];
            const double py = transform[1][0] * x + transform[1][1] * y + transform[1][2] * z + transform[1][3];
            const double pz = transform[2][0] * x + transform[2][1] * y + transform[2][2] * z + transform[2][3];
            Require(std::isfinite(px) && std::isfinite(py) && std::isfinite(pz), "Hit vertex is not finite.");
            return { static_cast<float>(px), static_cast<float>(py), static_cast<float>(-pz) };
        }

        struct AccessorView
        {
            const uint8_t* data = nullptr;
            size_t count = 0;
            size_t stride = 0;
            int componentType = 0;
        };

        size_t ComponentSize(int componentType)
        {
            if (componentType == 5121) return 1;
            if (componentType == 5123) return 2;
            if (componentType == 5125 || componentType == 5126) return 4;
            throw std::runtime_error("Unsupported accessor component type.");
        }

        AccessorView View(const Json& json, const std::vector<std::vector<uint8_t>>& buffers,
            size_t accessorIndex, const char* expectedType, int expectedComponents)
        {
            const Json& accessors = json.at("accessors");
            const Json& views = json.at("bufferViews");
            Require(accessorIndex < accessors.size(), "Accessor index is outside the array.");
            const Json& accessor = accessors[accessorIndex];
            Require(!accessor.contains("sparse") && accessor.at("type") == expectedType,
                "Unsupported HitMesh accessor layout.");
            const size_t viewIndex = accessor.at("bufferView").get<size_t>();
            Require(viewIndex < views.size(), "Buffer view index is outside the array.");
            const Json& view = views[viewIndex];
            const size_t bufferIndex = view.at("buffer").get<size_t>();
            Require(bufferIndex < buffers.size(), "Buffer index is outside the array.");
            const int componentType = accessor.at("componentType").get<int>();
            const size_t elementSize = ComponentSize(componentType) * expectedComponents;
            const size_t stride = view.value("byteStride", elementSize);
            Require(stride >= elementSize, "Buffer view stride is too small.");
            const size_t offset = view.value("byteOffset", size_t(0)) + accessor.value("byteOffset", size_t(0));
            const size_t count = accessor.at("count").get<size_t>();
            const size_t required = count == 0 ? offset : offset + stride * (count - 1) + elementSize;
            Require(required <= buffers[bufferIndex].size(), "Accessor reads outside its buffer.");
            return { buffers[bufferIndex].data() + offset, count, stride, componentType };
        }

        uint32_t ReadIndex(const AccessorView& view, size_t index)
        {
            const uint8_t* data = view.data + index * view.stride;
            if (view.componentType == 5121) return *data;
            if (view.componentType == 5123)
            {
                uint16_t value;
                std::memcpy(&value, data, sizeof(value));
                return value;
            }
            if (view.componentType == 5125)
            {
                uint32_t value;
                std::memcpy(&value, data, sizeof(value));
                return value;
            }
            throw std::runtime_error("HitMesh indices must be unsigned integers.");
        }

        void AppendNodeMesh(const Json& json, const std::vector<std::vector<uint8_t>>& buffers,
            size_t nodeIndex, const Matrix& world, HitTriangleMesh& output)
        {
            const Json& node = json.at("nodes").at(nodeIndex);
            const size_t meshIndex = node.at("mesh").get<size_t>();
            const Json& meshes = json.at("meshes");
            Require(meshIndex < meshes.size(), "Mesh index is outside the array.");
            for (const Json& primitive : meshes[meshIndex].at("primitives"))
            {
                Require(primitive.value("mode", 4) == 4, "HitMesh primitives must use TRIANGLES mode.");
                const size_t positionIndex = primitive.at("attributes").at("POSITION").get<size_t>();
                const AccessorView positions = View(json, buffers, positionIndex, "VEC3", 3);
                Require(positions.componentType == 5126, "HitMesh POSITION must use float components.");
                Require(primitive.contains("indices"), "HitMesh primitives must be indexed.");
                const AccessorView indices = View(json, buffers,
                    primitive.at("indices").get<size_t>(), "SCALAR", 1);
                Require(indices.count % 3 == 0, "HitMesh index count must be divisible by three.");
                const uint32_t vertexOffset = static_cast<uint32_t>(output.vertices.size());
                Require(uint64_t(vertexOffset) + positions.count <= UINT32_MAX, "HitMesh has too many vertices.");
                for (size_t vertex = 0; vertex < positions.count; ++vertex)
                {
                    std::array<float, 3> value;
                    std::memcpy(value.data(), positions.data + vertex * positions.stride, sizeof(value));
                    output.vertices.push_back(TransformPosition(world, value[0], value[1], value[2]));
                }
                for (size_t triangle = 0; triangle < indices.count; triangle += 3)
                {
                    const uint32_t a = ReadIndex(indices, triangle);
                    const uint32_t b = ReadIndex(indices, triangle + 1);
                    const uint32_t c = ReadIndex(indices, triangle + 2);
                    Require(a < positions.count && b < positions.count && c < positions.count,
                        "HitMesh index is outside POSITION.");
                    output.triangles.push_back({ vertexOffset + a, vertexOffset + c, vertexOffset + b });
                }
            }
        }
    }

    bool LoadGltfHitMesh(const std::filesystem::path& path, HitTriangleMesh& output, std::string& error)
    {
        try
        {
            GltfRoles roles;
            std::string roleError;
            Require(InspectGltfRoles(path, roles, roleError), roleError);
            const Document document = LoadDocument(path);
            const auto buffers = LoadBuffers(path, document);
            std::unordered_set<size_t> hitNodes;
            for (const RoleMeshNode& node : roles.meshNodes)
                if (node.role == MeshRole::Hit) hitNodes.insert(node.nodeIndex);

            const Json& nodes = document.json.at("nodes");
            const Json& scenes = document.json.at("scenes");
            const size_t sceneIndex = document.json.contains("scene") ?
                document.json.at("scene").get<size_t>() : 0;
            HitTriangleMesh result;
            std::function<void(size_t, const Matrix&)> visit;
            visit = [&](size_t nodeIndex, const Matrix& parent)
            {
                Require(nodeIndex < nodes.size(), "Node index is outside the array.");
                const Json& node = nodes[nodeIndex];
                const Matrix world = Multiply(parent, NodeTransform(node));
                if (hitNodes.contains(nodeIndex)) AppendNodeMesh(document.json, buffers, nodeIndex, world, result);
                if (node.contains("children"))
                    for (const Json& child : node.at("children")) visit(child.get<size_t>(), world);
            };
            for (const Json& root : scenes.at(sceneIndex).at("nodes")) visit(root.get<size_t>(), Identity());
            Require(!result.vertices.empty() && !result.triangles.empty(), "Hit role contains no triangle geometry.");
            output = std::move(result);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool TransformHitMesh(const HitTriangleMesh& input, const Transform& transform,
        HitTriangleMesh& output, std::string& error)
    {
        constexpr float degreesToRadians = 3.14159265358979323846f / 180.0f;
        const float pitch = transform.rotationDegrees[0] * degreesToRadians;
        const float yaw = transform.rotationDegrees[1] * degreesToRadians;
        const float roll = transform.rotationDegrees[2] * degreesToRadians;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        const float cr = std::cos(roll), sr = std::sin(roll);
        const float matrix[3][3] = {
            { cr * cy + sr * sp * sy, sr * cp, sr * sp * cy - cr * sy },
            { cr * sp * sy - sr * cy, cr * cp, sr * sy + cr * sp * cy },
            { cp * sy, -sp, cp * cy }
        };
        for (float value : transform.position)
        {
            if (!std::isfinite(value))
            {
                error = "HitMesh placement position must be finite.";
                return false;
            }
        }
        for (float value : transform.rotationDegrees)
        {
            if (!std::isfinite(value))
            {
                error = "HitMesh placement rotation must be finite.";
                return false;
            }
        }

        HitTriangleMesh result;
        result.triangles = input.triangles;
        result.vertices.reserve(input.vertices.size());
        for (const auto& vertex : input.vertices)
        {
            if (!std::isfinite(vertex[0]) || !std::isfinite(vertex[1]) || !std::isfinite(vertex[2]))
            {
                error = "HitMesh vertex must be finite before placement.";
                return false;
            }
            const float x = vertex[0] * matrix[0][0] + vertex[1] * matrix[1][0] +
                vertex[2] * matrix[2][0] + transform.position[0];
            const float y = vertex[0] * matrix[0][1] + vertex[1] * matrix[1][1] +
                vertex[2] * matrix[2][1] + transform.position[1];
            const float z = vertex[0] * matrix[0][2] + vertex[1] * matrix[1][2] +
                vertex[2] * matrix[2][2] + transform.position[2];
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            {
                error = "HitMesh placement produced a non-finite vertex.";
                return false;
            }
            result.vertices.push_back({ x, y, z });
        }
        output = std::move(result);
        error.clear();
        return true;
    }
}
