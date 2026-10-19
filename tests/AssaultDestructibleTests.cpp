#include "App/ManifestEnemySpawner.h"
#include "Map/MapHierarchy.h"
#include "Map/GltfHitMesh.h"
#include <filesystem>
#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    constexpr float dt = 1.0f / 60.0f;
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Assault destructibles: " << message << '\n';
        return condition;
    }
    void Advance(Tank::Physics::TrackedVehicleTest& world, int steps)
    {
        for (int i = 0; i < steps; ++i) world.Step(dt);
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    TrackedVehicleTest world;
    world.Initialize();
    Advance(world, 180);
    passed &= Check(!world.AddDestructibleBox({}, {0.0f, 1.0f, 1.0f}), "reject invalid size");
    passed &= Check(world.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f}, 40.0f), "create box");
    passed &= Check(world.FireAssault(), "manual fire succeeds");
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 40.0f &&
        world.State().assaultProjectiles.size() == 1, "manual shot spawns a round without instant damage");
    Advance(world, 3);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 40.0f &&
        world.State().assaultProjectiles[0].position.z > 5.0f, "round travels before reaching box");
    Advance(world, 17);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 20.0f &&
        world.State().assaultWeapon.roundsFired == 1 && world.State().assaultProjectiles.empty(),
        "impact damages once and releases slot");
    passed &= Check(world.State().assaultImpactMarks.Count() == 0, "box hit does not create a ground mark");
    TankInput input;
    input.fireAssault = true;
    world.SetInput(input);
    Advance(world, 12);
    passed &= Check(!world.State().destructibleBoxes[0].target.active, "held trigger destroys box");
    world.SetInput({});
    Advance(world, 10);
    auto unlimitedDistance = world.ProjectileSettings();
    unlimitedDistance.expireAtMaximumDistance = false;
    world.SetAssaultProjectileSettings(unlimitedDistance);
    world.FireAssault();
    Advance(world, 40);
    passed &= Check(!world.State().assaultProjectiles.empty() && world.State().assaultProjectiles.back().position.z > 40.0f,
        "destroyed collider is removed and rounds pass through without 40m cutoff");

    world.Initialize();
    passed &= Check(world.State().destructibleBoxes.empty() && world.State().assaultProjectiles.empty() && world.State().assaultWeapon.roundsFired == 0,
        "reset clears targets and weapon state");
    Advance(world, 180);
    world.AddDestructibleBox({0.0f, 1.5f, 10.0f}, {2.0f, 3.0f, 2.0f}, 20.0f);
    world.AddDestructibleBox({0.0f, 1.5f, 15.0f}, {2.0f, 3.0f, 2.0f}, 40.0f);
    world.FireAssault();
    Advance(world, 12);
    passed &= Check(!world.State().destructibleBoxes[0].target.active &&
        world.State().destructibleBoxes[1].target.hitPoints == 40.0f, "only closest box is damaged");
    Advance(world, 10);
    world.FireAssault();
    Advance(world, 12);
    passed &= Check(world.State().destructibleBoxes[1].target.hitPoints == 20.0f, "next shot reaches rear box");

    MapPrimitive wall;
    wall.position = {0.0f, 1.5f, 6.0f};
    wall.size = {3.0f, 3.0f, 1.0f};
    world.Initialize({}, {}, {wall});
    Advance(world, 180);
    world.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    Advance(world, 20);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 60.0f &&
        world.State().assaultHitTargetId == 0, "wall blocks damage");
    passed &= Check(world.State().assaultImpactMarks.Count() == 1,
        "static vertical wall receives an impact mark");
    const auto& wallMark = world.State().assaultImpactMarks.Slots().front();
    passed &= Check(std::abs(wallMark.normal.x) < 0.01f &&
        std::abs(wallMark.normal.y) < 0.01f && wallMark.normal.z < -0.99f,
        "vertical wall mark preserves the wall surface normal");

    world.Initialize();
    Advance(world, 180);
    world.AddDestructibleBox({0.0f, 1.5f, 55.0f}, {2.0f, 3.0f, 2.0f});
    world.SetAssaultProjectileSettings(unlimitedDistance);
    world.AddDestructibleBox({8.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    Advance(world, 50);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 40.0f &&
        world.State().destructibleBoxes[1].target.hitPoints == 60.0f, "infinite round reaches beyond 40m; off-axis box is missed");
    Tank::Map::HitTriangleMesh localMesh, authoredMesh, floor;
    std::string error;
    const auto templatePath = std::filesystem::path(__FILE__).parent_path().parent_path() /
        "Assets/Map/Templates/IndestructibleBox.gltf";
    passed &= Check(Tank::Map::LoadGltfHitMesh(templatePath, localMesh, error), "load authored box mesh");
    Tank::Map::Transform pose; pose.position = {0, 0, 12};
    passed &= Check(Tank::Map::TransformHitMesh(localMesh, pose, authoredMesh, error), "place authored collider");
    authoredMesh.placementId = "breakable-box"; authoredMesh.breakable = true; authoredMesh.hitPoints = 40;
    floor.vertices = {{-100,0,-100}, {100,0,-100}, {100,0,100}, {-100,0,100}};
    floor.triangles = {{0,3,2}, {0,2,1}};
    passed &= Check(world.InitializeWithStaticMeshes({}, {}, {floor, authoredMesh}, {}, error),
        "initialize breakable mesh without duplicate static collider");
    passed &= Check(world.State().destructibleBoxes.size() == 1 && world.State().fixedTurrets.empty() &&
        world.State().destructibleBoxes[0].placementId == "breakable-box", "mesh is a destructible target, not an enemy");
    Advance(world, 180);
    world.FireAssault(); Advance(world, 20);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 20, "ordinary round damages authored Hit mesh");
    world.FireAssault(); Advance(world, 20);
    passed &= Check(!world.State().destructibleBoxes[0].target.active, "authored mesh is destroyed at zero HP");
    world.FireAssault(); Advance(world, 25);
    passed &= Check(!world.State().assaultProjectiles.empty() && world.State().assaultProjectiles.back().position.z > 12,
        "destroyed authored collider is removed from physics");
    passed &= Check(world.InitializeWithStaticMeshes({}, {}, {floor, authoredMesh}, {}, error) &&
        world.State().destructibleBoxes[0].target.active && world.State().destructibleBoxes[0].target.hitPoints == 40,
        "reset restores authored collider and HP");
    TankSettings mortarSettings;
    mortarSettings.mortarMinimumFireAngleDegrees = 0;
    mortarSettings.mortarMinimumRangeMeters = mortarSettings.mortarMaximumRangeMeters = 12;
    mortarSettings.mortarMinimumAttackRadiusMeters = mortarSettings.mortarMaximumAttackRadiusMeters = 4;
    mortarSettings.mortarExplosionDamage = 200;
    passed &= Check(world.InitializeWithStaticMeshes(mortarSettings, {}, {floor, authoredMesh}, {}, error),
        "initialize authored mortar target");
    Advance(world, 60);
    TankInput mortarInput; mortarInput.leftLeverX = -1; mortarInput.rightLeverX = 1;
    world.SetInput(mortarInput); Advance(world, 10);
    mortarInput.fireAssault = true; world.SetInput(mortarInput); world.Step(dt); world.SetInput({});
    passed &= Check(world.State().mortarProjectiles.size() == 1, "launch mortar at authored mesh");
    for (int step = 0; step < 600 && !world.State().mortarProjectiles.empty(); ++step) world.Step(dt);
    passed &= Check(!world.State().destructibleBoxes[0].target.active, "mortar blast destroys authored mesh");
    authoredMesh.hitPoints = 20;
    passed &= Check(world.InitializeWithStaticMeshes({}, {}, {floor, authoredMesh}, {}, error), "initialize support test");
    Tank::Map::Manifest supportMap, supportWorld;
    supportMap.instances.push_back({"breakable-box", "Models/box.gltf", {{0,0,12}, {}}, "root", true, 20});
    supportMap.enemies.push_back({"supported", "Fixed turret", {0,3.5f,0}, {}, "mesh/breakable-box", true});
    supportMap.enemies.push_back({"independent", "Fixed turret", {3,3.5f,12}, {}});
    EnemyEditorSettings supportCatalog;
    supportCatalog.unitTypes[0].hitPoints = 90;
    supportCatalog.unitTypes[0].attackMounts.clear();
    passed &= Check(Tank::Map::ResolveHierarchy(supportMap, supportWorld, error) &&
        Tank::App::SpawnManifestEnemies(world, supportWorld, supportCatalog, error), "spawn authored parent-supported enemy");
    Advance(world, 180);
    passed &= Check(world.State().fixedTurrets[0].position.y == 3.5f && !world.State().fixedTurrets[0].supportReleased,
        "intact support keeps enemy in authored pose");
    world.FireAssault(); Advance(world, 25);
    passed &= Check(!world.State().destructibleBoxes[0].target.active && world.State().fixedTurrets[0].supportReleased,
        "destroyed support releases enemy into dynamic physics");
    Advance(world, 240);
    passed &= Check(world.State().fixedTurrets[0].position.y < 1 && world.State().fixedTurrets[0].position.y > 0.3f &&
        world.State().fixedTurrets[0].target.active && world.State().fixedTurrets[0].target.hitPoints == 90 &&
        world.State().fixedTurrets[1].position.y == 3.5f && !world.State().fixedTurrets[1].supportReleased,
        "supported enemy lands on ground with HP retained; opt-out enemy stays fixed");
    authoredMesh.hitPoints = 0;
    passed &= Check(!world.InitializeWithStaticMeshes({}, {}, {floor, authoredMesh}, {}, error), "reject invalid authored HP");
    if (!passed) return 1;
    std::cout << "PASS AssaultDestructible\n";
    return 0;
}
