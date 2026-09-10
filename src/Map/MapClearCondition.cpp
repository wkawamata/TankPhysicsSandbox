#include "MapClearCondition.h"

#include <cmath>

const Tank::Map::ClearArea* Tank::Map::FindContainingClearArea(
    const Manifest& manifest, const std::array<float, 3>& point)
{
    for (const ClearArea& area : manifest.clearAreas)
    {
        bool contains = true;
        for (size_t axis = 0; axis < point.size(); ++axis)
        {
            contains &= std::abs(point[axis] - area.center[axis]) <= area.size[axis] * 0.5f;
        }
        if (contains) return &area;
    }
    return nullptr;
}
