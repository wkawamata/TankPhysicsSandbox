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
    passed &= Check(world.State().assaultImpactMarks.Count() == 0, "vertical wall is not ground");

    world.Initialize();
    Advance(world, 180);
    world.AddDestructibleBox({0.0f, 1.5f, 55.0f}, {2.0f, 3.0f, 2.0f});
    world.SetAssaultProjectileSettings(unlimitedDistance);
    world.AddDestructibleBox({8.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    Advance(world, 50);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 40.0f &&
        world.State().destructibleBoxes[1].target.hitPoints == 60.0f, "infinite round reaches beyond 40m; off-axis box is missed");
    if (!passed) return 1;
    std::cout << "PASS AssaultDestructible\n";
    return 0;
}
