#pragma once

#include "PhysicsTypes.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Tank::Physics
{
    struct ProjectileIntercept
    {
        Vec3 point;
        float seconds = 0;
    };

    // Solve |target + velocity*t - origin| = projectileSpeed*t.
    inline std::optional<ProjectileIntercept> FindProjectileIntercept(
        const Vec3& origin, const Vec3& target, const Vec3& velocity,
        float projectileSpeed, float maximumSeconds)
    {
        const double x = double(target.x) - origin.x;
        const double y = double(target.y) - origin.y;
        const double z = double(target.z) - origin.z;
        const double vx = velocity.x, vy = velocity.y, vz = velocity.z;
        const double speed = projectileSpeed;
        const double a = vx*vx + vy*vy + vz*vz - speed*speed;
        const double b = 2*(x*vx + y*vy + z*vz);
        const double c = x*x + y*y + z*z;
        if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
            !std::isfinite(maximumSeconds) || speed <= 0 || maximumSeconds <= 0 || c < 1.0e-12)
            return std::nullopt;
        double time = std::numeric_limits<double>::infinity();
        if (std::abs(a) < 1.0e-10 * (vx*vx + vy*vy + vz*vz + speed*speed))
        {
            if (b < 0) time = -c / b;
        }
        else
        {
            const double discriminant = b*b - 4*a*c;
            if (discriminant < 0) return std::nullopt;
            const double q = -0.5 * (b + std::copysign(std::sqrt(discriminant), b));
            const double first = q / a;
            const double second = q != 0 ? c / q : -1;
            if (first > 0) time = first;
            if (second > 0) time = (std::min)(time, second);
        }
        if (!std::isfinite(time) || time > maximumSeconds) return std::nullopt;
        const Vec3 point = {float(target.x + vx*time), float(target.y + vy*time), float(target.z + vz*time)};
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return std::nullopt;
        return ProjectileIntercept{point, float(time)};
    }
}
