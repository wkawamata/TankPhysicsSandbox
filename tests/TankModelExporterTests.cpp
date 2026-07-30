#include "Rendering/TankModelExporter.h"
#include "Physics/TrackedVehicleTest.h"

#include <GltfLoader.h>
#include <Scene/SceneBuilder.h>
#include <DirectXMath.h>

#include <filesystem>
#include <iostream>

int main()
{
    Engine::SceneBuilder builder;
    const uint32_t material = builder.AddSolidColorMaterial(80, 140, 90, 255);
    builder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    builder.AddInstance(
        DirectX::XMMatrixScaling(2.0f, 1.0f, 3.0f) *
            DirectX::XMMatrixTranslation(4.0f, 2.0f, -3.0f),
        material);

    Tank::Physics::TrackedVehicleTestState state;
    state.bodyPosition = { 4.0f, 2.0f, -3.0f };
    const std::filesystem::path path =
        std::filesystem::current_path() / "TankModelExporterTests.glb";
    std::string status;
    const bool exported = Tank::Rendering::ExportTankGlb(
        builder.GetScene(), { { 0, "Hull" } }, state, path, status);

    GltfMeshData loaded;
    const bool loadedSuccessfully =
        exported && LoadGltfMesh(path.string(), loaded);
    std::filesystem::remove(path);
    if (!loadedSuccessfully || loaded.vertices.empty() || loaded.indices.empty())
    {
        std::cerr << "FAIL TankModelExporter: " << status << "\n";
        return 1;
    }
    std::cout << "PASS TankModelExporter\n";
    return 0;
}
