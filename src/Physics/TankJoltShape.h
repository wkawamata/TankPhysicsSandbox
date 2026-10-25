#pragma once

#include "TankCollisionProfile.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Tank::Physics
{
    JPH::RefConst<JPH::Shape> CreateTankJoltShape(
        const TankShapeSettings& settings);
}
