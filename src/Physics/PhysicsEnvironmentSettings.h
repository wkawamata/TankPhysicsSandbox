#pragma once

namespace Tank::Physics
{
    struct PhysicsEnvironmentSettings
    {
        float floorSizeM = 200.0f;
        float floorFriction = 0.6f;
        bool gridEnabled = true;
        float gridSpacingM = 5.0f;
    };
}
