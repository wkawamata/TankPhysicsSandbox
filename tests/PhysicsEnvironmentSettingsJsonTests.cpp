#include "Physics/PhysicsEnvironmentSettingsJson.h"

#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }
}

int main()
{
    Tank::Physics::PhysicsEnvironmentSettings source;
    source.floorSizeM = 350.0f;
    source.floorFriction = 0.65f;
    source.gridEnabled = false;
    source.gridSpacingM = 2.5f;
    source.obstacleCount = 35;
    source.obstacleSeed = 42;
    source.obstacleAreaSizeM = 180.0f;
    source.groundColor = { 0.25f, 0.5f, 0.75f };
    source.gridLineColor = { 0.8f, 0.2f, 0.4f };

    Tank::Physics::PhysicsEnvironmentSettings loaded;
    std::string error;
    if (!Tank::Physics::DeserializePhysicsEnvironmentSettings(
            Tank::Physics::SerializePhysicsEnvironmentSettings(source),
            loaded,
            &error) ||
        !NearlyEqual(loaded.floorSizeM, source.floorSizeM) ||
        !NearlyEqual(loaded.floorFriction, source.floorFriction) ||
        loaded.gridEnabled != source.gridEnabled ||
        !NearlyEqual(loaded.gridSpacingM, source.gridSpacingM) ||
        loaded.obstacleCount != source.obstacleCount ||
        loaded.obstacleSeed != source.obstacleSeed ||
        !NearlyEqual(loaded.obstacleAreaSizeM, source.obstacleAreaSizeM) ||
        !NearlyEqual(loaded.groundColor.r, source.groundColor.r) ||
        !NearlyEqual(loaded.groundColor.g, source.groundColor.g) ||
        !NearlyEqual(loaded.groundColor.b, source.groundColor.b) ||
        !NearlyEqual(loaded.gridLineColor.r, source.gridLineColor.r) ||
        !NearlyEqual(loaded.gridLineColor.g, source.gridLineColor.g) ||
        !NearlyEqual(loaded.gridLineColor.b, source.gridLineColor.b))
    {
        std::cerr << "FAIL PhysicsEnvironmentSettings JSON: " << error << "\n";
        return 1;
    }

    const Tank::Physics::PhysicsEnvironmentSettings beforeFutureVersion = loaded;
    if (Tank::Physics::DeserializePhysicsEnvironmentSettings(
            R"({"version":999,"floorSizeM":1.0})",
            loaded,
            &error) ||
        !NearlyEqual(loaded.floorSizeM, beforeFutureVersion.floorSizeM))
    {
        std::cerr << "FAIL PhysicsEnvironmentSettings future version\n";
        return 1;
    }

    Tank::Physics::PhysicsEnvironmentSettings oldVersion;
    oldVersion.floorSizeM = 123.0f;
    if (!Tank::Physics::DeserializePhysicsEnvironmentSettings(
            R"({"version":1,"floorSizeM":123.0})",
            oldVersion,
            &error) ||
        !NearlyEqual(oldVersion.groundColor.r, 0.384f) ||
        !NearlyEqual(oldVersion.gridLineColor.g, 0.620f))
    {
        std::cerr << "FAIL PhysicsEnvironmentSettings missing color keys keep defaults\n";
        return 1;
    }

    std::cout << "PASS PhysicsEnvironmentSettings JSON\n";
    return 0;
}
