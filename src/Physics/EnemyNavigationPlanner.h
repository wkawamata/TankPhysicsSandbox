#pragma once

#include "EnemyGroupBehavior.h"
#include "NavigationGrid.h"
#include <limits>
#include <optional>

namespace Tank::Physics
{
    struct NavigationObservation { Vec3 position; std::string surfaceId; };

    struct EnemyNavigationPlan
    {
        bool stopped = true;
        std::optional<NavigationPath> path;
        std::string error;
        std::uint64_t searches = 0;
    };

    // Plans from authored walkable surfaces. Unknown surfaces never fall back to another layer.
    class EnemyNavigationPlanner
    {
    public:
        bool Configure(const NavigationGrid& grid, std::span<const NavigationMobility> members, double snapDistance)
        {
            std::string error;
            if (!ValidateNavigationGrid(grid, error) || members.empty() || !std::isfinite(snapDistance) || snapDistance <= 0 ||
                !std::all_of(members.begin(), members.end(), IsValidNavigationMobility)) return false;
            m_grid = grid;
            m_members.assign(members.begin(), members.end());
            m_snapDistance = snapDistance;
            m_configured = true;
            m_lastStart.reset(); m_lastGoal.reset();
            m_state = {};
            return true;
        }

        void Update(const EnemyGroupBehaviorSnapshot& behavior, const Vec3& anchor, const std::string& anchorSurface)
        {
            if (!behavior.hasMoveTarget)
            {
                m_state.stopped = true;
                m_state.path.reset(); m_state.error.clear();
                m_lastStart.reset(); m_lastGoal.reset();
                return;
            }
            const auto start = Resolve(anchor, anchorSurface);
            const auto goal = Resolve(behavior.moveTarget, behavior.targetSurfaceId);
            if (!m_configured || !start || !goal)
            {
                m_state.stopped = true;
                m_state.path.reset();
                m_state.error = "Cannot resolve start or target on the specified walkable surface";
                m_lastStart.reset(); m_lastGoal.reset();
                return;
            }
            if (start == m_lastStart && goal == m_lastGoal) return;
            m_lastStart = start; m_lastGoal = goal;
            ++m_state.searches;
            NavigationPath path;
            if (!FindNavigationPath(m_grid, *start, *goal, m_members, path, m_state.error))
            {
                m_state.stopped = true;
                m_state.path.reset();
                return;
            }
            m_state.stopped = false;
            m_state.path = std::move(path);
        }

        std::optional<NavigationObservation> ResolveObservation(const Vec3& position, float supportHeight,
            float radiusMeters, float heightToleranceMeters) const
        {
            return ResolveObservation(position, Vec3{position.x, supportHeight, position.z}, radiusMeters, heightToleranceMeters);
        }

        std::optional<NavigationObservation> ResolveObservation(const Vec3& position, const Vec3& supportPoint,
            float radiusMeters, float heightToleranceMeters) const
        {
            if (!m_configured || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(supportPoint.x) || !std::isfinite(supportPoint.y) || !std::isfinite(supportPoint.z) ||
                !std::isfinite(radiusMeters) || radiusMeters <= 0 || !std::isfinite(heightToleranceMeters) || heightToleranceMeters <= 0) return {};
            const NavigationCell* selected = nullptr;
            double closest = (std::numeric_limits<double>::infinity)();
            bool ambiguous = false;
            for (const auto& cell : m_grid.cells)
            {
                if (!cell.passable || !cell.supportHeightMeters) continue;
                const double dx = double(position.x)-cell.position.x, dz = double(position.z)-cell.position.z;
                const double distance = std::hypot(dx, dz);
                if (distance > radiusMeters) continue;
                const double supportDx = double(supportPoint.x)-cell.position.x, supportDz = double(supportPoint.z)-cell.position.z;
                if (std::hypot(supportDx, supportDz) > radiusMeters) continue;
                const double expectedHeight = *cell.supportHeightMeters + cell.supportGradientXZ[0]*supportDx + cell.supportGradientXZ[1]*supportDz;
                if (!std::isfinite(expectedHeight) || std::abs(expectedHeight-supportPoint.y) > heightToleranceMeters) continue;
                if (distance < closest - 0.0001)
                { selected = &cell; closest = distance; ambiguous = false; }
                else if (std::abs(distance-closest) <= 0.0001 && selected && selected->surfaceId != cell.surfaceId)
                    ambiguous = true;
            }
            if (!selected || ambiguous) return {};
            return NavigationObservation{selected->position, selected->surfaceId};
        }
        const EnemyNavigationPlan& State() const { return m_state; }

    private:
        std::optional<size_t> Resolve(const Vec3& position, const std::string& surface) const
        {
            if (surface.empty() || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
                return {};
            std::optional<size_t> result;
            double closest = (std::numeric_limits<double>::infinity)();
            bool ambiguous = false;
            for (size_t i = 0; i < m_grid.cells.size(); ++i)
            {
                if (m_grid.cells[i].surfaceId != surface) continue;
                const double distance = NavigationDistance(position, m_grid.cells[i].position);
                if (distance > m_snapDistance) continue;
                if (distance < closest) { closest = distance; result = i; ambiguous = false; }
                else if (distance == closest) ambiguous = true;
            }
            return ambiguous ? std::optional<size_t>{} : result;
        }
        NavigationGrid m_grid;
        std::vector<NavigationMobility> m_members;
        double m_snapDistance = 0;
        bool m_configured = false;
        std::optional<size_t> m_lastStart, m_lastGoal;
        EnemyNavigationPlan m_state;
    };
}
