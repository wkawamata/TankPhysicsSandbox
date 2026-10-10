#include "Physics/TrackedVehicleTest.h"
#include "App/ManifestEnemySpawner.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    constexpr float dt = 1.0f / 60.0f;
    void Advance(Tank::Physics::TrackedVehicleTest& world, int count)
    {
        for (int i = 0; i < count; ++i) world.Step(dt);
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    auto check = [&](bool condition, const char* message)
    {
        if (!condition) { std::cerr << "FAIL FixedTurret: " << message << '\n'; passed = false; }
    };
    TrackedVehicleTest world;
    check(!world.AddFixedTurret({}, {2, 3, 2}, 40), "requires initialized world");
    world.Initialize();
    Advance(world, 180);
    check(!world.AddFixedTurret({}, {0, 3, 2}, 40) &&
        !world.AddFixedTurret({}, {2, 3, 2}, 0) &&
        !world.AddFixedTurret({std::numeric_limits<float>::quiet_NaN(), 0, 0}, {2, 3, 2}, 40) &&
        !world.AddFixedTurret({}, {2, std::numeric_limits<float>::infinity(), 2}, 40) &&
        !world.AddFixedTurret({}, {2, 3, 2}, std::numeric_limits<float>::infinity()), "reject invalid tuning");
    check(world.State().fixedTurrets.empty(), "invalid additions leave state unchanged");
    check(world.AddDestructibleBox({8, 1.5f, 12}, {2, 3, 2}, 40), "existing box creation");
    check(world.AddFixedTurret({0, 1.5f, 12}, {2, 3, 2}, 40), "create turret");
    check(world.AddDestructibleBox({0, 1.5f, 18}, {2, 3, 2}, 40), "rear box creation");
    const auto turretId = world.State().fixedTurrets[0].target.id;
    check(turretId != world.State().destructibleBoxes[0].target.id &&
        turretId != world.State().destructibleBoxes[1].target.id &&
        world.State().fixedTurrets[0].target.kind == CombatTargetKind::Enemy, "distinct target identity and Enemy kind");
    Advance(world, 60);
    const auto& turret = world.State().fixedTurrets[0];
    check(turret.position.x == 0 && turret.position.y == 1.5f && turret.position.z == 12, "stationary turret");
    check(world.FireAssault(), "first shot");
    Advance(world, 20);
    check(world.State().fixedTurrets[0].target.hitPoints == 20 && world.State().fixedTurrets[0].target.active,
        "ordinary shot applies existing 20 damage");
    check(world.State().destructibleBoxes[1].target.hitPoints == 40 && world.State().assaultImpactMarks.Count() == 0,
        "turret blocks rear box and receives no persistent surface mark");
    check(world.FireAssault(), "second shot");
    Advance(world, 20);
    check(!world.State().fixedTurrets[0].target.active && world.State().fixedTurrets[0].target.hitPoints == 0,
        "second shot destroys turret");
    check(world.FireAssault(), "third shot");
    Advance(world, 20);
    check(world.State().destructibleBoxes[1].target.hitPoints == 20, "destroyed turret collider removed");

    MapPrimitive wall;
    wall.position = {0, 1.5f, 6};
    wall.size = {3, 3, 1};
    world.Initialize({}, {}, {wall});
    Advance(world, 180);
    world.AddFixedTurret({0, 1.5f, 12}, {2, 3, 2}, 40);
    world.FireAssault();
    Advance(world, 20);
    check(world.State().fixedTurrets[0].target.hitPoints == 40 && world.State().assaultImpactMarks.Count() == 1,
        "wall prevents turret damage and retains surface mark");
    world.Initialize();
    check(world.State().fixedTurrets.empty() && world.State().destructibleBoxes.empty(), "reset clears both target types");
    check(world.AddFixedTurret({0, 1.5f, 12}, {2, 3, 2}, 40) && world.State().fixedTurrets[0].target.id == 1,
        "new session restarts target IDs");
    check(!world.AddFixedTurret({}, {1, 1, 1}, 60, {0, 0, 0, 0}), "reject invalid rotation");
    const float half = std::sqrt(0.5f);
    EnemyUnitType authoredType;
    authoredType.name = "Custom turret";
    authoredType.attackMounts.push_back({{1, 2, 3}, 0});
    check(world.AddFixedTurret({8, 1, 8}, {1, 2, 3}, 60, {0, half, 0, half}, "enemy-2", authoredType), "rotated placement creation");
    Advance(world, 60);
    const auto& rotated = world.State().fixedTurrets.back();
    check(rotated.rotation.y == half && rotated.position.x == 8 && rotated.target.hitPoints == 60,
        "placement rotation and HP retained while stationary");
    check(rotated.placementId == "enemy-2" && rotated.unitType.name == "Custom turret" &&
        rotated.unitType.attackMounts.size() == 2, "authored identity and multiple mounts retained");
    world.Initialize();
    Advance(world, 180);
    world.AddFixedTurret({2, 1.5f, 12}, {6, 3, 0.2f}, 60, {0, half, 0, half});
    world.AddDestructibleBox({0, 1.5f, 18}, {2, 3, 2}, 40);
    world.FireAssault();
    Advance(world, 30);
    check(world.State().fixedTurrets[0].target.hitPoints == 60 &&
        world.State().destructibleBoxes[0].target.hitPoints == 20, "rotated collider leaves shot path clear");

    Tank::Map::Manifest manifest;
    manifest.enemies.push_back({"enemy-1", "Fixed turret", {0, 1.5f, 12}, {0, 90, 0}});
    EnemyEditorSettings catalog;
    catalog.unitTypes[0].attackMounts.push_back({{1, 0, 0}, 0});
    std::string error;
    world.Initialize();
    auto invalidManifest = manifest;
    invalidManifest.enemies.push_back({"enemy-2", "missing", {}, {}});
    check(!Tank::App::SpawnManifestEnemies(world, invalidManifest, catalog, error) &&
        world.State().fixedTurrets.empty(), "unknown type rejects all placements before spawning");
    check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error), "spawn authored map");
    check(world.State().fixedTurrets[0].placementId == "enemy-1" &&
        world.State().fixedTurrets[0].unitType.attackMounts.size() == 2 &&
        std::abs(world.State().fixedTurrets[0].rotation.y - half) < 0.00001f, "map resolves type, mounts and rotation");
    Advance(world, 180);
    for (int shot = 0; shot < 3; ++shot) { world.FireAssault(); Advance(world, 20); }
    check(!world.State().fixedTurrets[0].target.active, "authored enemy is destructible");
    world.Initialize();
    check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
        world.State().fixedTurrets[0].target.hitPoints == 60 && world.State().fixedTurrets[0].target.active,
        "reset restores authored enemy HP and collider");
    // Destructor must release an active turret as well as destroyed ones.
    if (!passed) return 1;
    std::cout << "PASS FixedTurret\n";
}
