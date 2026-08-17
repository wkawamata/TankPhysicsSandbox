#pragma once

namespace Tank::Physics
{
    struct ColorRgb
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    struct PhysicsEnvironmentSettings
    {
        float floorSizeM = 200.0f;
        float floorFriction = 0.6f;
        bool gridEnabled = true;
        float gridSpacingM = 5.0f;
        int obstacleCount = 20;
        int obstacleSeed = 1;
        float obstacleAreaSizeM = 100.0f;
        ColorRgb groundColor = { 0.384f, 0.357f, 0.282f };
        ColorRgb gridLineColor = { 0.647f, 0.620f, 0.518f };
    };
}
