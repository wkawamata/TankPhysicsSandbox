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

        static SpecialAction Update(
            SpecialActionRecognizer& recognizer,
            const TankInput& input)
        {
            return recognizer.Update(FromTankInput(input));
        }
    };
}
