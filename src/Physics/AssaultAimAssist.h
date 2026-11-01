#pragma once

#include "AssaultWeapon.h"
#include "ProjectileIntercept.h"
#include <cstdint>
#include <span>

namespace Tank::Physics
{
    struct AssaultAimCandidate
    {
        std::uint64_t id = 0;
        Vec3 position;
        Vec3 velocity;
        bool projectile = false;
        float remainingSeconds = std::numeric_limits<float>::max();
        std::uint32_t bodyId = std::numeric_limits<std::uint32_t>::max();
    };
    struct AssaultAimSolution
    {
        std::uint64_t id = 0;
        Vec3 direction;
        Vec3 point;
    };

    // Forward defines the shot axis. The ellipse has independent horizontal and
    // vertical half-angles; zero constrains that axis to the centre line.
    inline bool InsideAssaultAimCone(const Vec3& offset, const Vec3& forward,
        float horizontalDegrees, float verticalDegrees)
    {
        const float length = std::hypot(forward.x, forward.y, forward.z);
        if (length <= 1.0e-6f || !std::isfinite(length)) return false;
        const Vec3 f = {forward.x/length, forward.y/length, forward.z/length};
        const float horizontalLength = std::hypot(f.x, f.z);
        const Vec3 right = horizontalLength > 1.0e-6f
            ? Vec3{f.z/horizontalLength, 0, -f.x/horizontalLength} : Vec3{1,0,0};
        const Vec3 up = {f.y*right.z-f.z*right.y, f.z*right.x-f.x*right.z, f.x*right.y-f.y*right.x};
        const float depth = offset.x*f.x + offset.y*f.y + offset.z*f.z;
        if (!std::isfinite(depth) || depth <= 1.0e-6f) return false;
        const float x = (offset.x*right.x + offset.y*right.y + offset.z*right.z) / depth;
        const float y = (offset.x*up.x + offset.y*up.y + offset.z*up.z) / depth;
        constexpr float radians = 3.14159265358979323846f / 180.0f;
        const auto contribution = [](float slope, float degrees)
        {
            if (degrees <= 0) return std::abs(slope) < 1.0e-6f ? 0.0f : std::numeric_limits<float>::infinity();
            const float radius = std::tan(std::clamp(degrees, 0.0f, 80.0f) * radians);
            return (slope/radius)*(slope/radius);
        };
        return contribution(x, horizontalDegrees) + contribution(y, verticalDegrees) <= 1.0f + 1.0e-6f;
    }

    template<class Visible>
    std::optional<AssaultAimSolution> FindAssaultAim(const Vec3& origin, const Vec3& forward,
        const AssaultProjectileSettings& settings, std::span<const AssaultAimCandidate> candidates,
        Visible visible)
    {
        if (!settings.aimAssistEnabled || settings.speedMetersPerSecond <= 0 ||
            !std::isfinite(settings.aimAssistDistanceMeters) || settings.aimAssistDistanceMeters <= 0)
            return std::nullopt;
        std::optional<AssaultAimSolution> selected;
        bool selectedProjectile = false;
        float nearest = std::numeric_limits<float>::max();
        float maximumSeconds = settings.aimAssistDistanceMeters / settings.speedMetersPerSecond;
        if (settings.expireAtMaximumDistance)
            maximumSeconds = (std::min)(maximumSeconds, settings.maximumDistanceMeters / settings.speedMetersPerSecond);
        if (settings.lifetimeSeconds > 0)
            maximumSeconds = (std::min)(maximumSeconds, settings.lifetimeSeconds);
        for (const auto& candidate : candidates)
        {
            const Vec3 offset = {candidate.position.x-origin.x, candidate.position.y-origin.y, candidate.position.z-origin.z};
            const float distance = std::hypot(offset.x, offset.y, offset.z);
            if (!std::isfinite(distance) || distance > settings.aimAssistDistanceMeters ||
                !InsideAssaultAimCone(offset, forward, settings.aimAssistHorizontalDegrees, settings.aimAssistVerticalDegrees)) continue;
            const auto intercept = FindProjectileIntercept(origin, candidate.position, candidate.velocity,
                settings.speedMetersPerSecond, (std::min)(maximumSeconds, candidate.remainingSeconds));
            if (!intercept) continue;
            const Vec3 aim = {intercept->point.x-origin.x, intercept->point.y-origin.y, intercept->point.z-origin.z};
            if (!InsideAssaultAimCone(aim, forward, settings.aimAssistHorizontalDegrees, settings.aimAssistVerticalDegrees)) continue;
            if (selected && (selectedProjectile > candidate.projectile ||
                (selectedProjectile == candidate.projectile && (distance > nearest ||
                    (distance == nearest && candidate.id >= selected->id))))) continue;
            if (!visible(candidate, intercept->point)) continue;
            const float aimLength = std::hypot(aim.x, aim.y, aim.z);
            if (aimLength <= 1.0e-6f) continue;
            selected = AssaultAimSolution{candidate.id, {aim.x/aimLength, aim.y/aimLength, aim.z/aimLength}, intercept->point};
            selectedProjectile = candidate.projectile;
            nearest = distance;
        }
        return selected;
    }
}
