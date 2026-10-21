#include "Physics/TankHitRegionQuery.h"
#include "Physics/TankTypes.h"
#include "Physics/PhysicsWorld.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    using namespace Tank::Physics;
    PhysicsWorld physicsWorld;
    physicsWorld.Initialize();
    bool passed = true;
    const auto check = [&](bool condition, const char* message)
    {
        if (!condition)
        {
            passed = false;
            std::cerr << "FAIL Tank hit region: " << message << '\n';
        }
    };

    std::vector<TankHitRegion> regions = {
        {"Hull", {TankShapeKind::Box, {}, {}, {2, 1, 4}}},
        {"Turret", {TankShapeKind::Capsule, {0, 1, 0}, {}, {1, 1, 2}}}
    };
    TransformState tank;
    tank.position = {10, 0, 5};
    const auto hull = TraceTankHitRegions({10, 0, 0}, {10, 0, 10},
        tank, regions);
    check(hull && hull->regionIndex == 0 &&
        std::abs(hull->position.z - 3.0f) < 0.001f,
        "box returns the nearest world-space impact");

    const auto turret = TraceTankHitRegions({10, 1, 0}, {10, 1, 10},
        tank, std::span(regions).subspan(1));
    check(turret && std::abs(turret->position.z - 4.0f) < 0.001f,
        "Z-axis capsule is independently hittable");

    tank.rotation = {0, std::sqrt(0.5f), 0, std::sqrt(0.5f)};
    const auto rotated = TraceTankHitRegions({5, 0, 5}, {15, 0, 5},
        tank, std::span(regions).first(1));
    check(rotated && std::abs(rotated->position.x - 8.0f) < 0.001f,
        "tank rotation transforms child hit regions");

    return passed ? 0 : 1;
}
