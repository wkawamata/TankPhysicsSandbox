#pragma once

#include "TankTypes.h"

namespace Tank::Physics
{
    TankMotionObservation BuildTankMotionObservation(const TankState& state);
}
