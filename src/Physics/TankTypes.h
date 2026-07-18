#pragma once

#include "PhysicsTypes.h"

namespace Tank::Physics
{
    struct TankInput
    {
        float throttle = 0.0f;
        float steering = 0.0f;
        float leftTrack = 0.0f;
        float rightTrack = 0.0f;
        bool brake = false;
    };

    struct TransformState
    {
        Vec3 position = {};
        Quat rotation = {};
    };

    struct TankState
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        TransformState body = {};
        Vec3 linearVelocity = {};
        Vec3 angularVelocity = {};
        bool sleeping = false;
    };
}
