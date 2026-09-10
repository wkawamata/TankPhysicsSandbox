#pragma once

#include "Physics/PhysicsTypes.h"
#include <algorithm>
#include <cmath>

namespace Tank::Rendering
{
    struct MortarRangeCue
    {
        Physics::Vec3 center = {};
        float radiusMeters = 0.0f;
        bool visible = false;
        bool canFire = false;

        void Sanitize()
        {
            if (!std::isfinite(radiusMeters)) radiusMeters = 0.0f;
            radiusMeters = std::clamp(radiusMeters, 0.0f, 1000.0f);
            if (!visible) canFire = false;
        }
    };
}
