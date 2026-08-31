#pragma once

#include "Physics/PhysicsTypes.h"

namespace Tank::Rendering
{
    struct MortarRangeCue
    {
        Physics::Vec3 center = {};
        float radiusMeters = 0.0f;
        bool visible = false;
        bool canFire = false;
    };
}
