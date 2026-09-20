#include "Physics/TrackedVehicleTest.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    constexpr float dt = 1.0f / 60.0f;
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Assault projectiles: " << message << '\n';
        return condition;
    }
    void Advance(Tank::Physics::TrackedVehicleTest& world, int steps)
    {
        for (int i = 0; i < steps; ++i) world.Step(dt);
    }
    void Reset(Tank::Physics::TrackedVehicleTest& world)
    {
        world.Initialize();
        Advance(world, 180);
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    TrackedVehicleTest world;
    Reset(world);
    passed &= Check(world.ProjectileSettings().lifetimeSeconds == 0.0f, "lifetime defaults to infinite");
    world.SetAssaultProjectileSettings({2, 10.0f, 7.0f, 0.0f, 128, false});
    world.FireAssault();
    Advance(world, 10);
    world.FireAssault();
    Advance(world, 10);
    passed &= Check(!world.FireAssault(), "capacity blocks manual fire");
    TankInput input;
    input.fireAssault = true;
    world.SetInput(input);
    Advance(world, 120);
    world.SetInput({});
    passed &= Check(world.State().assaultProjectiles.size() == 2 && world.State().assaultWeapon.roundsFired == 2,
        "capacity blocks automatic fire without increasing round counter");
    const float youngerAge = world.State().assaultProjectiles.back().ageSeconds;
    world.SetAssaultProjectileSettings({1, 100.0f, 50.0f, 0.01f});
    passed &= Check(world.State().assaultProjectiles.size() == 1 &&
        world.State().assaultProjectiles[0].ageSeconds == youngerAge &&
        world.State().assaultProjectiles[0].velocity.z == 10.0f &&
        world.State().assaultProjectiles[0].damage == 7.0f && world.State().assaultProjectiles[0].lifetimeSeconds == 0.0f,
        "lowering capacity removes oldest; in-flight rounds preserve launch settings");
    world.SetAssaultProjectileSettings({2, 100.0f, 50.0f, 0.01f});
    passed &= Check(world.FireAssault() && world.State().assaultProjectiles.back().velocity.z == 100.0f &&
        world.State().assaultProjectiles.back().damage == 50.0f &&
        world.State().assaultProjectiles.back().lifetimeSeconds == 0.01f,
        "live tuning applies to newly fired rounds");
    Advance(world, 600);
    passed &= Check(world.State().assaultProjectiles.size() == 1, "infinite lifetime survives ten seconds");
    world.SetAssaultProjectileSettings({0, 80.0f, 20.0f, 0.0f});
    world.SetInput(input);
    Advance(world, 10);
    passed &= Check(world.State().assaultProjectiles.empty() && !world.FireAssault() &&
        world.State().assaultWeapon.roundsFired == 3, "zero capacity clears and disables rounds");

    Reset(world);
    world.SetAssaultProjectileSettings({1, 10.0f, 7.0f, 0.1f});
    world.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    world.Step(dt);
    passed &= Check(std::abs(world.State().assaultProjectiles[0].velocity.z - 10.0f) < 0.001f, "configured speed is applied");
    Advance(world, 10);
    passed &= Check(world.State().assaultProjectiles.empty() && world.FireAssault() &&
        world.State().destructibleBoxes[0].target.hitPoints == 60.0f, "expiry releases slot before distant target");

    Reset(world);
    world.SetAssaultProjectileSettings({1, 10.0f, 7.0f, 0.1f});
    world.SetInput(input);
    Advance(world, 60);
    passed &= Check(world.State().assaultWeapon.roundsFired > 3 && world.State().assaultProjectiles.size() <= 1,
        "held trigger resumes after capacity is released by expiry");

    Reset(world);
    world.SetAssaultProjectileSettings({2, 10000.0f, 7.0f, 0.0f});
    world.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 0.1f});
    world.FireAssault();
    world.Step(dt);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 53.0f && world.State().assaultProjectiles.empty(),
        "swept segment prevents tunneling and uses configured damage");

    Reset(world);
    world.SetAssaultProjectileSettings({2, 10000.0f, 20.0f, 0.001f});
    world.AddDestructibleBox({0.0f, 1.5f, 25.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    world.Step(dt);
    passed &= Check(world.State().destructibleBoxes[0].target.hitPoints == 60.0f && world.State().assaultProjectiles.empty(),
        "lifetime clips travel within final step");
    world.SetAssaultProjectileSettings({-1, std::numeric_limits<float>::quiet_NaN(), -10.0f, std::numeric_limits<float>::infinity()});
    passed &= Check(world.ProjectileSettings().maximumCount == 0 && world.ProjectileSettings().speedMetersPerSecond == 80.0f &&
        world.ProjectileSettings().damagePerRound == 0.0f && world.ProjectileSettings().lifetimeSeconds == 0.0f,
        "invalid settings are sanitized");
    Reset(world);
    passed &= Check(world.ProjectileSettings().expireAtMaximumDistance &&
        world.ProjectileSettings().maximumDistanceMeters == 40.0f, "distance expiry defaults on at 40m");
    auto muzzleSettings = world.ProjectileSettings();
    muzzleSettings.muzzleLocalPosition = {0.5f, 1.25f, 2.0f};
    world.SetAssaultProjectileSettings(muzzleSettings);
    const Vec3 bodyPosition = world.State().bodyPosition;
    world.FireAssault();
    passed &= Check(std::abs(world.State().assaultProjectiles.back().position.x -
            (bodyPosition.x + 0.5f)) < 0.01f &&
        std::abs(world.State().assaultProjectiles.back().position.y -
            (bodyPosition.y + 1.25f)) < 0.01f &&
        std::abs(world.State().assaultProjectiles.back().position.z -
            (bodyPosition.z + 2.0f)) < 0.01f,
        "GUI muzzle XYZ is transformed from tank local coordinates");
    Reset(world);
    world.SetAssaultProjectileSettings({1, 10000.0f, 20.0f, 0.0f, 128, true, 5.0f});
    world.AddDestructibleBox({0.0f, 1.5f, 12.0f}, {2.0f, 3.0f, 2.0f});
    world.FireAssault();
    world.Step(dt);
    passed &= Check(world.State().assaultProjectiles.empty() &&
        world.State().destructibleBoxes[0].target.hitPoints == 60.0f,
        "maximum distance clips segment before target and releases active count");
    Advance(world, 10);
    passed &= Check(world.FireAssault(), "distance expiry frees capacity for next round after cooldown");
    Reset(world);
    world.SetAssaultProjectileSettings({1, 80.0f, 20.0f, 0.0f, 128, false, 5.0f});
    world.FireAssault();
    Advance(world, 60);
    passed &= Check(world.State().assaultProjectiles.size() == 1 &&
        world.State().assaultProjectiles[0].distanceTraveledMeters > 5.0f,
        "flag off permits infinite lifetime beyond maximum distance");
    if (!passed) return 1;
    std::cout << "PASS AssaultProjectile\n";
    return 0;
}
