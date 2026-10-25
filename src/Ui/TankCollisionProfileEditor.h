#pragma once

#include "Physics/TankCollisionProfile.h"

namespace Ui
{
    bool DrawTankCollisionProfileEditor(
        const char* id,
        Tank::Physics::TankCollisionProfile& profile);
}
