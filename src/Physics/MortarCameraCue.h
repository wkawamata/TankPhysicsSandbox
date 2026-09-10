#pragma once

#include <algorithm>

namespace Tank::Physics
{
    struct MortarCameraCue
    {
        float progress = 0.0f;
        float pitchOffsetDegrees = 0.0f;
        bool rangeVisibilityRequired = false;

        static MortarCameraCue FromWheelieProgress(float progress)
        {
            const float clamped = std::clamp(progress, 0.0f, 1.0f);
            return { clamped, 18.0f * clamped, clamped > 0.0f };
        }
    };
}
