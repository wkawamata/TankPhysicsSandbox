#include "Physics/AssaultAimAssist.h"
#include "Physics/TrackedVehicleTest.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void Check(bool value, const char* message)
    { if (!value) throw std::runtime_error(message); }
}
int main()
{
    using namespace Tank::Physics;
    try
    {
        const auto stationary = FindProjectileIntercept({}, {0,0,20}, {}, 80, 1);
        Check(stationary && std::abs(stationary->seconds - 0.25f) < 0.00001f,
            "stationary target uses projectile flight time");
        const auto lateral = FindProjectileIntercept({}, {0,0,20}, {10,0,0}, 80, 1);
        Check(lateral && lateral->point.x > 2.5f &&
            std::abs(std::hypot(lateral->point.x, lateral->point.y, lateral->point.z) - 80*lateral->seconds) < 0.001f,
            "moving projectile is led to its future position");
        Check(!FindProjectileIntercept({}, {0,0,20}, {0,0,100}, 80, 10),
            "unreachable receding target is rejected");
        const auto equalSpeed = FindProjectileIntercept({}, {0,0,20}, {0,0,-80}, 80, 1);
        Check(equalSpeed && std::abs(equalSpeed->seconds - 0.125f) < 0.00001f,
            "equal-speed approaching projectile uses the linear solution");
        Check(!FindProjectileIntercept({}, {0,0,20}, {}, 80, 0.1f),
            "target beyond the shot lifetime is rejected");
        Check(!FindProjectileIntercept({}, {0,0,20}, {}, 0, 1) &&
            !FindProjectileIntercept({}, {0,0,20}, {std::numeric_limits<float>::quiet_NaN(),0,0}, 80, 1),
            "invalid inputs are rejected");
        AssaultProjectileSettings settings;
        settings.aimAssistHorizontalDegrees = 20;
        settings.aimAssistVerticalDegrees = 5;
        const Vec3 forward = {0,0,1};
        Check(InsideAssaultAimCone({3,0,10}, forward, 20, 5) &&
            !InsideAssaultAimCone({0,1,10}, forward, 20, 5), "cone axes are independent");
        Check(!InsideAssaultAimCone({3,0.8f,10}, forward, 20, 5), "elliptical cone rejects diagonal outside its edge");
        Check(!InsideAssaultAimCone({0,0,-10}, forward, 20, 5), "targets behind the muzzle are rejected");
        Check(InsideAssaultAimCone({2,0,10}, forward, 20, 0) &&
            !InsideAssaultAimCone({0,0.1f,10}, forward, 20, 0), "zero half-angle constrains one axis");
        std::vector<AssaultAimCandidate> candidates = {
            {1, {0,0,3}, {}, false}, {2, {1,0,12}, {}, true}, {3, {1,0,8}, {}, true}};
        const auto visible = [](const auto&, const auto&) { return true; };
        auto aim = FindAssaultAim({}, forward, settings, candidates, visible);
        Check(aim && aim->id == 3, "nearest enemy projectile wins even over a closer tank");
        aim = FindAssaultAim({}, forward, settings, candidates,
            [](const auto& candidate, const auto&) { return candidate.id != 3; });
        Check(aim && aim->id == 2, "blocked nearest target falls back to the next visible projectile");
        candidates.resize(1);
        aim = FindAssaultAim({}, forward, settings, candidates, visible);
        Check(aim && aim->id == 1, "enemy tank is the fallback when no projectile is eligible");
        settings.aimAssistEnabled = false;
        Check(!FindAssaultAim({}, forward, settings, candidates, visible), "disabled assist preserves the shot direction");
        settings.aimAssistEnabled = true;
        settings.lifetimeSeconds = 0.01f;
        Check(!FindAssaultAim({}, forward, settings, candidates, visible), "short-lived shots do not lock unreachable targets");
        settings.lifetimeSeconds = 0;
        candidates = {{4, {1,0,10}, {2,0,0}, true}};
        aim = FindAssaultAim({}, forward, settings, candidates, visible);
        Check(aim && aim->point.x > 1 && aim->direction.x > 0, "assist leads moving projectiles");

        candidates = {{5, {0,0,50}, {}, true}, {6, {0,0,-10}, {}, true}};
        Check(!FindAssaultAim({}, forward, settings, candidates, visible), "out-of-range and behind targets do not attract shots");
        candidates = {{7, {0,0,10}, {100,0,0}, true}};
        Check(!FindAssaultAim({}, forward, settings, candidates, visible), "unreachable projectile is ignored");
        candidates = {{8, {1,0,10}, {}, true}};
        Check(!FindAssaultAim({}, forward, settings, candidates,
            [](const auto&, const auto&) { return false; }), "fully blocked cone preserves the original shot direction");

        TrackedVehicleTest world;
        const auto prepare = [&]
        {
            world.Initialize();
            for (int i=0; i<180; ++i) world.Step(1.0f/60);
            EnemyAttackType attack;
            attack.maximumYawSpeedDegreesPerSecond = 360;
            attack.projectileSpeedMetersPerSecond = 1;
            attack.firingIntervalSeconds = 0.1f;
            Check(world.ConfigureEnemyAttacks({attack}), "attack catalog configures");
            Check(world.AddFixedTurret({1.5f,1.5f,12}, {1,1,1}, 60, {0,1,0,0}), "off-axis enemy spawns");
            for (int i=0; i<60 && world.State().enemyProjectiles.empty(); ++i) world.Step(1.0f/60);
            Check(!world.State().enemyProjectiles.empty(), "enemy round is in flight");
            // Let the first projectile clear the firing tank's hull.
            for (int i=0; i<240; ++i) world.Step(1.0f/60);
            auto config = world.ProjectileSettings();
            config.aimAssistHorizontalDegrees = config.aimAssistVerticalDegrees = 30;
            config.muzzleLocalPosition.y = 1.5f - world.State().bodyPosition.y;
            world.SetAssaultProjectileSettings(config);
        };
        prepare();
        const auto enemyId = world.State().enemyProjectiles.front().target.id;
        Check(world.FireAssault() && world.State().assaultProjectiles.back().velocity.x > 0.1f,
            "manual fire uses the cone correction at spawn");
        for (int i=0; i<20; ++i) world.Step(1.0f/60);
        Check(std::none_of(world.State().enemyProjectiles.begin(), world.State().enemyProjectiles.end(),
            [enemyId](const auto& projectile) { return projectile.target.id == enemyId; }),
            "corrected shot intercepts the selected enemy projectile");
        prepare();
        TankInput input; input.fireAssault = true;
        world.SetInput(input); world.Step(1.0f/60);
        Check(!world.State().assaultProjectiles.empty() && world.State().assaultProjectiles.back().velocity.x > 0.1f,
            "held-fire path applies the same aim correction");
        prepare();
        MapPrimitive wall; wall.position = {0,1.5f,6}; wall.size = {8,3,0.2f};
        // Insert cover after the enemy has already fired.
        world.AddDestructibleBox(wall.position, wall.size, 100);
        Check(world.FireAssault() && std::abs(world.State().assaultProjectiles.back().velocity.x) < 0.001f,
            "blocked targets do not steer the projectile through cover");
        std::cout << "PASS AssaultAimAssist\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
