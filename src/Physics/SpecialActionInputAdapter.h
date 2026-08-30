#pragma once

#include "SpecialActionRecognizer.h"
#include "TankTypes.h"

namespace Tank::Physics
{
    struct SpecialActionInputAdapter
    {
        static SpecialActionInput FromTankInput(const TankInput& input)
        {
            return { input.leftLeverX, input.rightLeverX };
        }
    };
}
