#pragma once

namespace Tank::App
{
    constexpr bool ShouldLoadAndResetRollingProfile(
        int currentSlot,
        int selectedSlot,
        bool autoLoadAndReset)
    {
        return autoLoadAndReset && currentSlot != selectedSlot;
    }
}
