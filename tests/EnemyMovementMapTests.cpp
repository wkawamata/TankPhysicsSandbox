#include "App/ManifestEnemySpawner.h"
#include "App/EnemyNavigationLoader.h"
#include "Map/GltfHitMesh.h"
#include "Physics/EnemyEditorJson.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main()
{
    try
    {
        const auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path()/"Assets/Map/EnemyMovementTest";
        const auto read = [](const auto& path) { std::ifstream input(path, std::ios::binary);
            if (!input) throw std::runtime_error("Missing test-map file");
            return std::string(std::istreambuf_iterator<char>(input), {}); };
        std::string error;
        Tank::Map::Manifest map;
        Tank::Physics::EnemyEditorSettings catalog;
        check(Tank::Map::DeserializeManifest(read(folder/"Manifest.json"), map, error), "Load map manifest");
        check(Tank::Physics::DeserializeEnemyEditor(read(folder/"enemy_types.json"), catalog, error), "Load map-local enemy settings");
        std::optional<Tank::Physics::EnemyNavigationSetup> navigation;
        check(Tank::App::LoadEnemyNavigationSetup(folder, map, navigation, error) && navigation &&
            navigation->drivers.size() == 3, "Validate all authored patrol cycles");
        std::vector<Tank::Map::HitTriangleMesh> meshes;
        for (const auto& instance : map.instances)
        {
            Tank::Map::HitTriangleMesh local, transformed;
            check(Tank::Map::LoadGltfHitMesh(folder/instance.asset, local, error), "Read course collision geometry");
            check(Tank::Map::TransformHitMesh(local, instance.transform, transformed, error), "Transform course geometry");
            meshes.push_back(std::move(transformed));
        }
        check(std::any_of(meshes[0].vertices.begin(), meshes[0].vertices.end(), [](const auto& v)
            { return std::abs(v[0]-25)<0.001f && std::abs(v[1]-4)<0.001f && std::abs(v[2]-20)<0.001f; }),
            "Bridge collision geometry matches world-space navigation coordinates");
        Tank::Physics::TrackedVehicleTest world;
        Tank::Physics::MapSpawn spawn;
        spawn.position = {-20, 2, -45};
        const auto reset = [&] {
            check(world.InitializeWithStaticMeshes({}, {}, meshes, spawn, error), "Initialize actual map terrain");
            check(Tank::App::SpawnManifestEnemies(world, map, catalog, error) &&
                world.ConfigureEnemyMovement(*navigation, error) && world.SetEnemySimulationSettings(catalog.simulation),
                "Spawn configured moving tank models"); };
        const auto advance = [&](int frames) { for (int i=0;i<frames;++i) world.Step(1.0f/60); };
        reset();
        advance(120);
        check(world.State().fixedTurrets[0].position.z > 2 && !world.State().fixedTurrets[0].physicalMoving,
            "Initial distant patrol moves with simple navigation");
        check(world.State().fixedTurrets[2].position.y > 2 && !world.State().fixedTurrets[2].physicalMoving,
            "Bridge patrol climbs the authored ramp in simple mode");
        check(world.State().bodyPosition.y > 0.5f && world.State().bodyPosition.y < 1.5f,
            "Player rests on actual course ground");
        spawn.position = {-20, 2, -15};
        reset();
        advance(180);
        check(world.State().fixedTurrets[0].physicalMoving && world.State().fixedTurrets[0].runningGear,
            "Moving closer enables tracked physics on the loaded map");
        spawn.position = {15, 2, -15};
        reset();
        advance(1800);
        check(world.State().fixedTurrets[1].position.x > 1 &&
            world.State().enemyGroups[1].hasLastKnownPosition && world.State().playerCombat.hitPoints < 100,
            "Authored pursuit station detects, moves, and fires at the player");
        spawn.position = {30, 2, -10};
        reset();
        advance(480);
        check(world.State().fixedTurrets[2].physicalMoving && world.State().fixedTurrets[2].position.z > 7 &&
            world.State().fixedTurrets[2].position.y > 2,
            "Actual nearby tracked enemy climbs the course ramp");
        spawn.position = {30, 2, 0};
        check(world.InitializeWithStaticMeshes({}, {}, meshes, spawn, error), "Initialize player ramp regression");
        advance(120);
        Tank::Physics::TankInput drive;
        drive.throttle = 0.35f;
        for (int i=0; i<1800 && world.State().bodyPosition.z < 25; ++i)
        { world.SetInput(drive); world.Step(1.0f/60); }
        check(world.State().bodyPosition.z >= 25 && world.State().bodyPosition.y > 4 && world.DriverInput().forward > 0,
            "Player accepts forward input while crossing the actual uphill ramp");
        drive = {}; drive.brake = true; world.SetInput(drive); advance(120);
        drive = {}; drive.throttle = -0.35f;
        for (int i=0; i<1800 && world.State().bodyPosition.z > 10; ++i)
        { world.SetInput(drive); world.Step(1.0f/60); }
        check(world.State().bodyPosition.z <= 10 && world.DriverInput().forward < 0,
            "Player accepts reverse input from the slope without reset");
        spawn.position = {30, 3, 10};
        check(world.InitializeWithStaticMeshes({}, {}, meshes, spawn, error), "Initialize slope gesture regression");
        advance(120);
        drive = {}; drive.throttle = 0.5f; world.SetInput(drive); advance(30);
        auto mortarGesture = drive;
        mortarGesture.leftLeverX = -1; mortarGesture.rightLeverX = 1;
        world.SetInput(mortarGesture); world.Step(1.0f/60);
        check(world.State().specialMove.state == Tank::Physics::SpecialMoveState::MortarStarting,
            "Brief gamepad-like mortar gesture reserves movement on slope");
        drive = {}; drive.throttle = -0.5f;
        world.SetInput(drive); world.Step(1.0f/60);
        check(world.State().specialMove.state == Tank::Physics::SpecialMoveState::Idle &&
            world.DriverInput().forward < 0 && world.DriverInput().brake == 0,
            "Releasing unfinished mortar gesture restores reverse input on slope immediately");
        std::cout << "PASS EnemyMovementMap\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}