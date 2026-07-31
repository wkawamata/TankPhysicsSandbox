#include "Rendering/TankModelExporter.h"
#include "Physics/TrackedVehicleTest.h"

#include <GltfLoader.h>
#include <Scene/SceneBuilder.h>
#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

int main()
{
    Engine::SceneBuilder builder;
    const uint32_t material = builder.AddSolidColorMaterial(80, 140, 90, 255);
    builder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    builder.AddInstance(
        DirectX::XMMatrixScaling(2.0f, 1.0f, 3.0f) *
            DirectX::XMMatrixTranslation(4.0f, 2.0f, -3.0f),
        material);
    const Engine::SceneMeshId cylinder =
        builder.AddCylinder(1.0f, 1.0f, 16, Engine::CylinderCapMode::Both);
    builder.AddInstance(
        cylinder,
        DirectX::XMMatrixScaling(0.5f, 0.8f, 0.5f) *
            DirectX::XMMatrixTranslation(5.0f, 2.0f, -3.0f),
        material);

    Tank::Physics::TrackedVehicleTestState state;
    state.bodyPosition = { 4.0f, 2.0f, -3.0f };
    const std::filesystem::path path =
        std::filesystem::current_path() / "TankModelExporterTests.gltf";
    std::string status;
    const bool exported = Tank::Rendering::ExportTankGltf(
        builder.GetScene(),
        { { 0, "Hull" }, { 1, "Wheel" } },
        state,
        path,
        false,
        status);

    GltfMeshData loaded;
    const bool loadedSuccessfully =
        exported && LoadGltfMesh(path.string(), loaded);
    nlohmann::json json;
    if (exported)
    {
        std::ifstream input(path, std::ios::binary);
        input >> json;
    }
    const bool jsonValid =
        json.is_object() &&
        json["asset"]["version"] == "2.0" &&
        json["nodes"].is_array() &&
        json["nodes"].size() == 2 &&
        json["meshes"].is_array() &&
        json["meshes"].size() == 2 &&
        json["meshes"][0]["primitives"][0]["mode"] ==
            TINYGLTF_MODE_TRIANGLES &&
        json["meshes"][1]["primitives"][0]["mode"] ==
            TINYGLTF_MODE_TRIANGLES &&
        json["buffers"].is_array() &&
        json["buffers"].size() == 1 &&
        json["buffers"][0]["uri"].is_string() &&
        !json["buffers"][0]["uri"].get<std::string>().starts_with("data:");
    const std::filesystem::path binaryPath =
        path.parent_path() / json["buffers"][0].value("uri", "");
    if (std::getenv("TANK_KEEP_EXPORT_TEST") == nullptr)
    {
        std::filesystem::remove(path);
        std::filesystem::remove(binaryPath);
    }
    if (!loadedSuccessfully || !jsonValid ||
        loaded.vertices.empty() || loaded.indices.empty())
    {
        std::cerr << "FAIL TankModelExporter: " << status << "\n";
        return 1;
    }
    std::cout << "PASS TankModelExporter\n";
    return 0;
}
