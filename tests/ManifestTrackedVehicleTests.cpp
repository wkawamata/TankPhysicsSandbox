#include "Map/GltfHitMesh.h"
#include "Map/MapManifest.h"
#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        constexpr float dt = 1.0f / 60.0f;
        const std::filesystem::path mapFolder =
            std::filesystem::path(__FILE__).parent_path().parent_path() /
            "Config" / "Maps" / "MapEditorPreview";
        std::ifstream input(mapFolder / "Manifest.json", std::ios::binary);
        Check(static_cast<bool>(input), "Open MapEditorPreview Manifest");
        const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Tank::Map::Manifest manifest;
        std::string error;
        Check(Tank::Map::DeserializeManifest(json, manifest, error), "Read MapEditorPreview Manifest");

        std::vector<Tank::Map::HitTriangleMesh> meshes;
        for (const Tank::Map::Instance& instance : manifest.instances)
        {
            const std::u8string assetUtf8(instance.asset.begin(), instance.asset.end());
            Tank::Map::HitTriangleMesh local;
            Check(Tank::Map::LoadGltfHitMesh(mapFolder / std::filesystem::path(assetUtf8), local, error),
                "Load Manifest HitMesh");
            Tank::Map::HitTriangleMesh world;
            Check(Tank::Map::TransformHitMesh(local, instance.transform, world, error),
                "Apply Manifest placement to HitMesh");
            meshes.push_back(std::move(world));
        }
        Check(!meshes.empty(), "Preview map contains at least one HitMesh");

        Tank::Physics::MapSpawn spawn;
        spawn.position = { 4.0f, 2.0f, -3.0f };
        Tank::Physics::TrackedVehicleTest vehicle;
        Check(vehicle.InitializeWithStaticMeshes({}, {}, meshes, spawn, error),
            "Initialize tracked vehicle using only Manifest HitMeshes");
        const Tank::Physics::TrackedVehicleTestState firstStep = vehicle.Step(dt);
        Check(std::abs(firstStep.bodyPosition.x - spawn.position.x) < 0.05f &&
            std::abs(firstStep.bodyPosition.z - spawn.position.z) < 0.05f,
            "Tank starts at the requested non-origin Manifest position");
        for (int step = 1; step < 180; ++step) vehicle.Step(dt);
        const Tank::Physics::TrackedVehicleTestState settled = vehicle.State();
        Check(std::isfinite(settled.bodyPosition.y) && settled.bodyPosition.y > 0.5f &&
            settled.bodyPosition.y < 3.0f, "Tank settles on the glTF ground without the default floor");
        bool hasContact = false;
        for (int wheel = 0; wheel < settled.wheelCount; ++wheel)
            hasContact |= settled.wheels[static_cast<size_t>(wheel)].hasContact;
        Check(hasContact, "Tank wheels contact the Manifest HitMesh");

        Tank::Physics::TankInput drive;
        drive.throttle = 0.5f;
        vehicle.SetInput(drive);
        for (int step = 0; step < 60; ++step) vehicle.Step(dt);
        const Tank::Physics::TrackedVehicleTestState driven = vehicle.State();
        Check(driven.bodyPosition.z > settled.bodyPosition.z + 0.05f,
            "Tank drives forward on the Manifest HitMesh");
        Check(driven.bodyPosition.y > 0.3f, "Tank remains above the HitMesh while driving");
        std::cout << "Manifest tracked vehicle physics test passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
