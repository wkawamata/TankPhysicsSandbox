#include "stdafx.h"
#include "Rendering/TankModelExporter.h"

#include "Physics/TrackedVehicleTest.h"
#include <Scene/Scene.h>
#include <tiny_gltf.h>

#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <system_error>

namespace Tank::Rendering
{
    namespace
    {
        template<typename T>
        size_t Append(std::vector<unsigned char>& data, const std::vector<T>& values)
        {
            while ((data.size() & 3u) != 0u)
            {
                data.push_back(0);
            }
            const size_t offset = data.size();
            const size_t bytes = values.size() * sizeof(T);
            data.resize(offset + bytes);
            if (bytes > 0)
            {
                std::memcpy(data.data() + offset, values.data(), bytes);
            }
            return offset;
        }

        int AddAccessor(
            tinygltf::Model& model,
            int bufferView,
            int componentType,
            int type,
            size_t count,
            const std::vector<double>& minimum = {},
            const std::vector<double>& maximum = {})
        {
            tinygltf::Accessor accessor;
            accessor.bufferView = bufferView;
            accessor.componentType = componentType;
            accessor.type = type;
            accessor.count = count;
            accessor.minValues = minimum;
            accessor.maxValues = maximum;
            model.accessors.push_back(accessor);
            return static_cast<int>(model.accessors.size() - 1);
        }
    }

    bool ExportTankGltf(
        const Engine::Scene& scene,
        const std::vector<TankExportPart>& parts,
        const Tank::Physics::TrackedVehicleTestState& state,
        const std::filesystem::path& path,
        bool binary,
        std::string& status)
    {
        if (scene.mesh == nullptr)
        {
            status = "Export failed: scene has no mesh";
            return false;
        }

        using namespace DirectX;
        const XMMATRIX bodyWorld =
            XMMatrixRotationQuaternion(XMVectorSet(
                state.bodyRotation.x,
                state.bodyRotation.y,
                state.bodyRotation.z,
                state.bodyRotation.w)) *
            XMMatrixTranslation(
                state.bodyPosition.x,
                state.bodyPosition.y,
                state.bodyPosition.z);
        const XMMATRIX inverseBody = XMMatrixInverse(nullptr, bodyWorld);

        tinygltf::Model model;
        model.asset.version = "2.0";
        model.asset.generator = "Tank Physics Sandbox";
        model.buffers.emplace_back();
        std::vector<unsigned char>& bufferData = model.buffers[0].data;

        for (const Engine::SceneMaterial& source : scene.mesh->materials)
        {
            tinygltf::Material material;
            material.name = "TankMaterial_" + std::to_string(model.materials.size());
            if (source.albedoTexIndex >= 0 &&
                static_cast<size_t>(source.albedoTexIndex) < scene.mesh->textures.size())
            {
                const Engine::SceneTexture& texture =
                    scene.mesh->textures[static_cast<size_t>(source.albedoTexIndex)];
                if (texture.pixels.size() >= 4)
                {
                    material.pbrMetallicRoughness.baseColorFactor = {
                        texture.pixels[0] / 255.0,
                        texture.pixels[1] / 255.0,
                        texture.pixels[2] / 255.0,
                        texture.pixels[3] / 255.0 };
                }
            }
            material.pbrMetallicRoughness.metallicFactor = source.metallicFactor;
            material.pbrMetallicRoughness.roughnessFactor = source.roughnessFactor;
            model.materials.push_back(material);
        }

        tinygltf::Scene outputScene;
        for (const TankExportPart& part : parts)
        {
            if (part.instanceIndex >= scene.instances.size())
            {
                continue;
            }
            const Engine::InstanceData& instance = scene.instances[part.instanceIndex];
            if (instance.meshId >= scene.mesh->ranges.size())
            {
                continue;
            }
            const Engine::SceneMesh::Range& range =
                scene.mesh->ranges[instance.meshId];
            if (range.vertexCount == 0 || range.indexCount == 0)
            {
                continue;
            }

            const XMMATRIX instanceWorld =
                XMMatrixTranspose(XMLoadFloat4x4(&instance.world));
            const XMMATRIX local = instanceWorld * inverseBody;
            const XMMATRIX normalMatrix =
                XMMatrixTranspose(XMMatrixInverse(nullptr, local));

            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> texcoords;
            positions.reserve(static_cast<size_t>(range.vertexCount) * 3);
            normals.reserve(static_cast<size_t>(range.vertexCount) * 3);
            texcoords.reserve(static_cast<size_t>(range.vertexCount) * 2);
            std::vector<double> minimum(3, std::numeric_limits<double>::max());
            std::vector<double> maximum(3, std::numeric_limits<double>::lowest());
            for (uint32_t i = 0; i < range.vertexCount; ++i)
            {
                const Engine::SceneVertex& vertex =
                    scene.mesh->vertices[range.firstVertex + i];
                XMFLOAT3 position;
                XMFLOAT3 normal;
                XMStoreFloat3(
                    &position,
                    XMVector3TransformCoord(XMLoadFloat3(&vertex.position), local));
                XMStoreFloat3(
                    &normal,
                    XMVector3Normalize(
                        XMVector3TransformNormal(XMLoadFloat3(&vertex.normal), normalMatrix)));
                position.z = -position.z;
                normal.z = -normal.z;
                positions.insert(positions.end(), { position.x, position.y, position.z });
                normals.insert(normals.end(), { normal.x, normal.y, normal.z });
                texcoords.insert(texcoords.end(), { vertex.uv.x, 1.0f - vertex.uv.y });
                const double values[] = { position.x, position.y, position.z };
                for (int axis = 0; axis < 3; ++axis)
                {
                    minimum[axis] = (std::min)(minimum[axis], values[axis]);
                    maximum[axis] = (std::max)(maximum[axis], values[axis]);
                }
            }

            std::vector<uint32_t> indices;
            indices.reserve(range.indexCount);
            for (uint32_t i = 0; i + 2 < range.indexCount; i += 3)
            {
                const uint32_t a =
                    scene.mesh->indices[range.firstIndex + i] -
                    range.firstVertex;
                const uint32_t b =
                    scene.mesh->indices[range.firstIndex + i + 1] -
                    range.firstVertex;
                const uint32_t c =
                    scene.mesh->indices[range.firstIndex + i + 2] -
                    range.firstVertex;
                indices.insert(indices.end(), { a, c, b });
            }

            auto addView = [&model, &bufferData](size_t offset, size_t size, int target)
            {
                tinygltf::BufferView view;
                view.buffer = 0;
                view.byteOffset = offset;
                view.byteLength = size;
                view.target = target;
                model.bufferViews.push_back(view);
                return static_cast<int>(model.bufferViews.size() - 1);
            };
            const size_t posOffset = Append(bufferData, positions);
            const int posView = addView(
                posOffset, positions.size() * sizeof(float),
                TINYGLTF_TARGET_ARRAY_BUFFER);
            const size_t normalOffset = Append(bufferData, normals);
            const int normalView = addView(
                normalOffset, normals.size() * sizeof(float),
                TINYGLTF_TARGET_ARRAY_BUFFER);
            const size_t uvOffset = Append(bufferData, texcoords);
            const int uvView = addView(
                uvOffset, texcoords.size() * sizeof(float),
                TINYGLTF_TARGET_ARRAY_BUFFER);
            const size_t indexOffset = Append(bufferData, indices);
            const int indexView = addView(
                indexOffset, indices.size() * sizeof(uint32_t),
                TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

            tinygltf::Primitive primitive;
            primitive.mode = TINYGLTF_MODE_TRIANGLES;
            primitive.attributes["POSITION"] = AddAccessor(
                model, posView, TINYGLTF_COMPONENT_TYPE_FLOAT,
                TINYGLTF_TYPE_VEC3, range.vertexCount, minimum, maximum);
            primitive.attributes["NORMAL"] = AddAccessor(
                model, normalView, TINYGLTF_COMPONENT_TYPE_FLOAT,
                TINYGLTF_TYPE_VEC3, range.vertexCount);
            primitive.attributes["TEXCOORD_0"] = AddAccessor(
                model, uvView, TINYGLTF_COMPONENT_TYPE_FLOAT,
                TINYGLTF_TYPE_VEC2, range.vertexCount);
            primitive.indices = AddAccessor(
                model, indexView, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT,
                TINYGLTF_TYPE_SCALAR, indices.size());
            primitive.material =
                instance.materialId < model.materials.size()
                ? static_cast<int>(instance.materialId)
                : -1;

            tinygltf::Mesh mesh;
            mesh.name = part.name;
            mesh.primitives.push_back(primitive);
            model.meshes.push_back(mesh);
            tinygltf::Node node;
            node.name = part.name;
            node.mesh = static_cast<int>(model.meshes.size() - 1);
            model.nodes.push_back(node);
            outputScene.nodes.push_back(static_cast<int>(model.nodes.size() - 1));
        }

        model.scenes.push_back(outputScene);
        model.defaultScene = 0;
        std::error_code errorCode;
        std::filesystem::create_directories(path.parent_path(), errorCode);
        if (errorCode)
        {
            status = "Export failed: " + errorCode.message();
            return false;
        }
        tinygltf::TinyGLTF writer;
        if (!writer.WriteGltfSceneToFile(
                &model, path.string(), false, false, true, binary))
        {
            status = "Export failed: writer error";
            return false;
        }
        status = "Exported: " + path.string();
        return true;
    }
}
