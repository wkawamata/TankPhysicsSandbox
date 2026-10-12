#pragma once

#include "NavigationGrid.h"

namespace Tank::Physics
{
    struct SimpleNavigationState
    {
        Vec3 position;
        std::string surfaceId;
        std::string nextSurfaceId;
        size_t segmentIndex = 0;
        double distanceOnSegment = 0;
        double speedMetersPerSecond = 0;
        bool arrived = false;
    };

    class SimpleNavigationMovement
    {
    public:
        bool Initialize(const NavigationPath& path, const NavigationMobility& mobility);
        bool Advance(double seconds, bool stopAtNextNode = false);
        const SimpleNavigationState& State() const { return m_state; }

    private:
        void RefreshState();
        NavigationPath m_path;
        NavigationMobility m_mobility;
        SimpleNavigationState m_state;
        bool m_initialized = false;
    };
}
