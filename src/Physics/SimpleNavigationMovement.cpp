#include "SimpleNavigationMovement.h"
#include <cmath>

namespace Tank::Physics
{
    bool SimpleNavigationMovement::Initialize(const NavigationPath& path, const NavigationMobility& mobility)
    {
        if (path.cells.empty() || path.passages.size() != path.cells.size() - 1 ||
            !IsValidNavigationMobility(mobility)) return false;
        for (const auto& cell : path.cells)
            if (!CanTraverseNavigationCell(cell, mobility)) return false;
        for (size_t i = 0; i < path.passages.size(); ++i)
            if (!CanTraverseNavigationPassage(path.cells[i], path.cells[i + 1], path.passages[i], mobility)) return false;
        m_path = path;
        m_mobility = mobility;
        m_state = {};
        m_initialized = true;
        RefreshState();
        return true;
    }

    void SimpleNavigationMovement::RefreshState()
    {
        const auto& from = m_path.cells[m_state.segmentIndex];
        m_state.surfaceId = from.surfaceId;
        m_state.arrived = m_state.segmentIndex == m_path.cells.size() - 1;
        if (m_state.arrived)
        {
            m_state.position = from.position;
            m_state.nextSurfaceId = from.surfaceId;
            m_state.speedMetersPerSecond = 0;
            return;
        }
        const auto& to = m_path.cells[m_state.segmentIndex + 1];
        m_state.nextSurfaceId = to.surfaceId;
        const double length = NavigationDistance(from.position, to.position);
        const double fraction = m_state.distanceOnSegment / length;
        m_state.position = {static_cast<float>(double(from.position.x) + (double(to.position.x) - from.position.x) * fraction),
            static_cast<float>(double(from.position.y) + (double(to.position.y) - from.position.y) * fraction),
            static_cast<float>(double(from.position.z) + (double(to.position.z) - from.position.z) * fraction)};
        const auto terrain = m_state.distanceOnSegment < length * 0.5 ? from.terrain : to.terrain;
        m_state.speedMetersPerSecond = double(m_mobility.speedMetersPerSecond) /
            m_mobility.terrainCosts[static_cast<size_t>(terrain)];
    }

    bool SimpleNavigationMovement::Advance(double seconds, bool stopAtNextNode)
    {
        if (!m_initialized || !std::isfinite(seconds) || seconds <= 0) return false;
        while (seconds > 0 && !m_state.arrived)
        {
            const double length = NavigationDistance(m_path.cells[m_state.segmentIndex].position,
                m_path.cells[m_state.segmentIndex + 1].position);
            const double boundary = m_state.distanceOnSegment < length * 0.5 ? length * 0.5 : length;
            const double time = (boundary - m_state.distanceOnSegment) / m_state.speedMetersPerSecond;
            if (seconds < time)
            {
                m_state.distanceOnSegment += seconds * m_state.speedMetersPerSecond;
                seconds = 0;
            }
            else
            {
                seconds -= time;
                m_state.distanceOnSegment = boundary;
                if (boundary == length)
                {
                    ++m_state.segmentIndex;
                    m_state.distanceOnSegment = 0;
                    if (stopAtNextNode) seconds = 0;
                }
            }
            RefreshState();
        }
        return true;
    }
}
