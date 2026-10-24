#include "TankHitRegionQuery.h"

#include "TankTypes.h"

#include <Jolt/Jolt.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Tank::Physics
{
    namespace
    {
        bool TraceSphere(JPH::Vec3Arg origin, JPH::Vec3Arg direction,
            JPH::Vec3Arg center, float radius, float& fraction,
            JPH::Vec3& normal)
        {
            const JPH::Vec3 offset = origin - center;
            const float a = direction.Dot(direction);
            const float b = offset.Dot(direction);
            const float c = offset.Dot(offset) - radius * radius;
            const float discriminant = b * b - a * c;
            if (a <= 1.0e-12f || discriminant < 0.0f) return false;
            const float candidate = (-b - std::sqrt(discriminant)) / a;
            if (candidate < 0.0f || candidate > 1.0f) return false;
            fraction = candidate;
            normal = (origin + candidate * direction - center).Normalized();
            return true;
        }

        bool TraceBox(JPH::Vec3Arg origin, JPH::Vec3Arg direction,
            JPH::Vec3Arg halfSize, float& fraction, JPH::Vec3& normal)
        {
            float enter = 0.0f;
            float leave = 1.0f;
            int enterAxis = 0;
            float enterSign = 0.0f;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float o = origin[axis];
                const float d = direction[axis];
                const float extent = halfSize[axis];
                if (std::abs(d) < 1.0e-8f)
                {
                    if (o < -extent || o > extent) return false;
                    continue;
                }
                float nearFraction = (-extent - o) / d;
                float farFraction = (extent - o) / d;
                float sign = -1.0f;
                if (nearFraction > farFraction)
                {
                    std::swap(nearFraction, farFraction);
                    sign = 1.0f;
                }
                if (nearFraction > enter)
                {
                    enter = nearFraction;
                    enterAxis = axis;
                    enterSign = sign;
                }
                leave = std::min(leave, farFraction);
                if (enter > leave) return false;
            }
            if (enter < 0.0f || enter > 1.0f) return false;
            fraction = enter;
            normal = JPH::Vec3::sZero();
            normal.SetComponent(enterAxis, enterSign);
            return true;
        }

        bool TraceCapsule(JPH::Vec3Arg origin, JPH::Vec3Arg direction,
            float radius, float halfCylinder, float& fraction,
            JPH::Vec3& normal)
        {
            const JPH::Vec3 a(0, 0, -halfCylinder);
            const JPH::Vec3 b(0, 0, halfCylinder);
            const JPH::Vec3 axis = b - a;
            const JPH::Vec3 offset = origin - a;
            const float axisLengthSquared = axis.Dot(axis);
            const float axisDirection = axis.Dot(direction);
            const float axisOffset = axis.Dot(offset);
            const float directionOffset = direction.Dot(offset);
            const float directionLengthSquared = direction.Dot(direction);
            const float offsetLengthSquared = offset.Dot(offset);
            const float qa = axisLengthSquared * directionLengthSquared -
                axisDirection * axisDirection;
            const float qb = axisLengthSquared * directionOffset -
                axisOffset * axisDirection;
            const float qc = axisLengthSquared * offsetLengthSquared -
                axisOffset * axisOffset - radius * radius * axisLengthSquared;
            const float discriminant = qb * qb - qa * qc;
            if (qa > 1.0e-12f && discriminant >= 0.0f)
            {
                const float candidate = (-qb - std::sqrt(discriminant)) / qa;
                const float height = axisOffset + candidate * axisDirection;
                if (candidate >= 0.0f && candidate <= 1.0f &&
                    height > 0.0f && height < axisLengthSquared)
                {
                    fraction = candidate;
                    const JPH::Vec3 center = a +
                        axis * (height / axisLengthSquared);
                    normal = (origin + candidate * direction - center).Normalized();
                    return true;
                }
            }
            float firstFraction = std::numeric_limits<float>::max();
            JPH::Vec3 firstNormal;
            for (const auto center : {a, b})
            {
                float candidate = 0.0f;
                JPH::Vec3 candidateNormal;
                if (TraceSphere(origin, direction, center, radius, candidate,
                        candidateNormal) && candidate < firstFraction)
                {
                    firstFraction = candidate;
                    firstNormal = candidateNormal;
                }
            }
            if (firstFraction > 1.0f) return false;
            fraction = firstFraction;
            normal = firstNormal;
            return true;
        }
    }

    std::optional<TankHitRegionTrace> TraceTankHitRegions(
        const Vec3& start, const Vec3& end,
        const TransformState& tankTransform,
        std::span<const TankHitRegion> regions)
    {
        const JPH::Quat bodyRotation(tankTransform.rotation.x,
            tankTransform.rotation.y, tankTransform.rotation.z,
            tankTransform.rotation.w);
        const JPH::Quat inverseBodyRotation = bodyRotation.Conjugated();
        const JPH::Vec3 tankOrigin = inverseBodyRotation * JPH::Vec3(
            start.x - tankTransform.position.x,
            start.y - tankTransform.position.y,
            start.z - tankTransform.position.z);
        const JPH::Vec3 tankDirection = inverseBodyRotation * JPH::Vec3(
            end.x - start.x, end.y - start.y, end.z - start.z);
        constexpr float radiansPerDegree = JPH::JPH_PI / 180.0f;

        std::optional<TankHitRegionTrace> closest;
        for (size_t index = 0; index < regions.size(); ++index)
        {
            const auto& settings = regions[index].shape;
            if (!IsValidTankShape(settings)) continue;
            const JPH::Quat localRotation = JPH::Quat::sEulerAngles(JPH::Vec3(
                settings.localRotationDegrees.x * radiansPerDegree,
                settings.localRotationDegrees.y * radiansPerDegree,
                settings.localRotationDegrees.z * radiansPerDegree));
            const JPH::Quat inverseLocalRotation = localRotation.Conjugated();
            const JPH::Vec3 origin = inverseLocalRotation *
                (tankOrigin - JPH::Vec3(settings.localPosition.x,
                    settings.localPosition.y, settings.localPosition.z));
            const JPH::Vec3 direction = inverseLocalRotation * tankDirection;
            float fraction = 0.0f;
            JPH::Vec3 localNormal;
            bool hit = false;
            if (settings.kind == TankShapeKind::Capsule)
            {
                const float xScale =
                    settings.sizeMeters.x / settings.sizeMeters.y;
                const JPH::Vec3 scaledOrigin(origin.GetX() / xScale,
                    origin.GetY(), origin.GetZ());
                const JPH::Vec3 scaledDirection(direction.GetX() / xScale,
                    direction.GetY(), direction.GetZ());
                hit = TraceCapsule(scaledOrigin, scaledDirection,
                    0.5f * settings.sizeMeters.y,
                    0.5f * (settings.sizeMeters.z - settings.sizeMeters.y),
                    fraction, localNormal);
                if (hit)
                {
                    localNormal = JPH::Vec3(localNormal.GetX() / xScale,
                        localNormal.GetY(), localNormal.GetZ()).Normalized();
                }
            }
            else
            {
                hit = TraceBox(origin, direction,
                    JPH::Vec3(settings.sizeMeters.x, settings.sizeMeters.y,
                        settings.sizeMeters.z) * 0.5f,
                    fraction, localNormal);
            }
            if (!hit || (closest && fraction >= closest->fraction)) continue;
            const JPH::Vec3 worldNormal =
                bodyRotation * (localRotation * localNormal);
            closest = TankHitRegionTrace {
                fraction,
                index,
                {start.x + (end.x - start.x) * fraction,
                    start.y + (end.y - start.y) * fraction,
                    start.z + (end.z - start.z) * fraction},
                {worldNormal.GetX(), worldNormal.GetY(), worldNormal.GetZ()}
            };
        }
        return closest;
    }
}
