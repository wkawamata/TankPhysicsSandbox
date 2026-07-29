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

    Tank::Physics::PhysicsEnvironmentSettings loaded;
    std::string error;
    if (!Tank::Physics::DeserializePhysicsEnvironmentSettings(
            Tank::Physics::SerializePhysicsEnvironmentSettings(source),
            loaded,
            &error) ||
        !NearlyEqual(loaded.floorSizeM, source.floorSizeM) ||
        !NearlyEqual(loaded.floorFriction, source.floorFriction))
    {
        std::cerr << "FAIL PhysicsEnvironmentSettings JSON: " << error << "\n";
        return 1;
    }

    std::cout << "PASS PhysicsEnvironmentSettings JSON\n";
    return 0;
}
