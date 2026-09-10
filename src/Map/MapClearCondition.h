#pragma once

#include "MapManifest.h"

#include <array>

namespace Tank::Map
{
    // Returns the first AABB containing the point. AABB boundaries are inclusive.
    const ClearArea* FindContainingClearArea(
        const Manifest& manifest, const std::array<float, 3>& point);
}
