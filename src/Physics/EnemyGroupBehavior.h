#pragma once

#include "Map/MapManifest.h"
#include "PhysicsTypes.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Tank::Physics
{
    enum class EnemyGroupAction { Hold, Patrol, Approach, AttackStop, Search, SearchWait };

    struct EnemyGroupBehaviorSnapshot
    {
        EnemyGroupAction action = EnemyGroupAction::Hold;
        bool hasMoveTarget = false;
        Vec3 moveTarget = {};
        std::string targetSurfaceId;
        size_t patrolIndex = 0;
        float searchWaitRemaining = 0;
        // Intent only. Weapon range, visibility, aim and cooldown still apply.
        bool attackRequested = false;
    };

    class EnemyGroupBehavior
    {
    public:
        bool Initialize(const Map::EnemyPatrolSettings& settings)
        {
            if (!Map::IsValidEnemyPatrolSettings(settings)) return false;
            m_settings = settings;
            m_phase = EnemyGroupAction::Patrol;
            m_state = {};
            m_lastObserved = {};
            m_lastObservedNavigation = {};
            m_lastObservedSurface.clear();
            m_initialized = true;
            return true;
        }

        bool Update(float dt, bool active, const Vec3& anchor, bool detected, const Vec3& observedPosition,
            const std::string& observedSurface = {}, const Vec3* navigationPosition = nullptr)
        {
            const auto finite = [](const Vec3& p)
                { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
            if (!m_initialized || !std::isfinite(dt) || dt <= 0 || !finite(anchor) ||
                (detected && (!finite(observedPosition) || (navigationPosition && !finite(*navigationPosition))))) return false;
            m_state.hasMoveTarget = false;
            m_state.attackRequested = false;
            m_state.targetSurfaceId.clear();
            if (!active)
            {
                m_state.action = EnemyGroupAction::Hold;
                return true; // Pause phase and timer until command can resume.
            }
            if (detected)
            {
                m_lastObserved = observedPosition;
                m_lastObservedSurface = observedSurface;
                m_lastObservedNavigation = navigationPosition ? *navigationPosition : observedPosition;
                const double distance = Distance(anchor, observedPosition);
                if (m_phase == EnemyGroupAction::AttackStop && distance < m_settings.resumeApproachMeters)
                    m_phase = EnemyGroupAction::AttackStop;
                else
                    m_phase = distance <= m_settings.attackStopMeters ? EnemyGroupAction::AttackStop : EnemyGroupAction::Approach;
                m_state.searchWaitRemaining = 0;
            }
            else if (m_phase == EnemyGroupAction::Approach || m_phase == EnemyGroupAction::AttackStop)
                m_phase = EnemyGroupAction::Search;

            if (m_phase == EnemyGroupAction::Search)
            {
                if (Distance(anchor, m_lastObservedNavigation) <= m_settings.arrivalMeters)
                {
                    m_phase = EnemyGroupAction::SearchWait;
                    m_state.searchWaitRemaining = m_settings.searchWaitSeconds;
                }
                else { SetMoveTarget(m_lastObservedNavigation); m_state.targetSurfaceId = m_lastObservedSurface; }
            }
            else if (m_phase == EnemyGroupAction::SearchWait)
            {
                m_state.searchWaitRemaining = std::max(0.0f, m_state.searchWaitRemaining - dt);
                if (m_state.searchWaitRemaining <= 0) m_phase = EnemyGroupAction::Patrol;
            }
            if (m_phase == EnemyGroupAction::SearchWait && m_state.searchWaitRemaining <= 0)
                m_phase = EnemyGroupAction::Patrol;
            if (m_phase == EnemyGroupAction::Patrol)
            {
                // Consume at most one waypoint per update, including repeated points.
                auto point = m_settings.points[m_state.patrolIndex];
                if (Distance(anchor, Position(point)) <= m_settings.arrivalMeters)
                {
                    m_state.patrolIndex = (m_state.patrolIndex + 1) % m_settings.points.size();
                    point = m_settings.points[m_state.patrolIndex];
                }
                SetMoveTarget(Position(point));
                m_state.targetSurfaceId = point.surfaceId;
            }
            else if (m_phase == EnemyGroupAction::Approach)
            { SetMoveTarget(m_lastObservedNavigation); m_state.targetSurfaceId = m_lastObservedSurface; }
            else if (m_phase == EnemyGroupAction::AttackStop) m_state.attackRequested = true;
            m_state.action = m_phase;
            return true;
        }

        const EnemyGroupBehaviorSnapshot& State() const { return m_state; }

    private:
        static Vec3 Position(const Map::EnemyPatrolPoint& point)
            { return {point.position[0], point.position[1], point.position[2]}; }
        static double Distance(const Vec3& a, const Vec3& b)
            { return std::hypot(double(a.x) - b.x, double(a.y) - b.y, double(a.z) - b.z); }
        void SetMoveTarget(const Vec3& position)
            { m_state.hasMoveTarget = true; m_state.moveTarget = position; }

        Map::EnemyPatrolSettings m_settings;
        EnemyGroupBehaviorSnapshot m_state;
        EnemyGroupAction m_phase = EnemyGroupAction::Patrol;
        Vec3 m_lastObserved;
        Vec3 m_lastObservedNavigation;
        std::string m_lastObservedSurface;
        bool m_initialized = false;
    };
}
