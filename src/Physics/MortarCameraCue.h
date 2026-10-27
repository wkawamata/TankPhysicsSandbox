#pragma once

#include <algorithm>

namespace Tank::Physics
{
    struct MortarCameraCue
    {
        float progress = 0.0f;
        float pitchOffsetDegrees = 0.0f;
        bool rangeVisibilityRequired = false;

        static MortarCameraCue FromAimingState(bool canFire, bool atMaximum, float readyOffsetPercent)
        {
            return FromWheelieProgress(atMaximum ? 1.0f :
                (canFire ? std::clamp(readyOffsetPercent, 0.0f, 100.0f) / 100.0f : 0.0f));
        }

        static MortarCameraCue FromWheelieProgress(float progress)
        {
            const float clamped = std::clamp(progress, 0.0f, 1.0f);
            return { clamped, 18.0f * clamped, clamped > 0.0f };
        }
    };
}
