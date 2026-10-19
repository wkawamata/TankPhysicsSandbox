#pragma once

#include "MapManifest.h"

#include <array>

namespace Tank::Map
{
    // Returns the first transformed unit box containing the point. Boundaries are inclusive.
    const ClearArea* FindContainingClearArea(
        const Manifest& manifest, const std::array<float, 3>& point);
}
