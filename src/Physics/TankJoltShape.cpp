#include "TankJoltShape.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace Tank::Physics
{
    JPH::RefConst<JPH::Shape> CreateTankJoltShape(
        const TankShapeSettings& settings)
    {
        if (!IsValidTankShape(settings)) return nullptr;

        JPH::RefConst<JPH::Shape> shape;
        JPH::Quat shapeRotation = JPH::Quat::sIdentity();
        if (settings.kind == TankShapeKind::Capsule)
        {
            const float radius = 0.5f * settings.sizeMeters.x;
            const float halfCylinder =
                0.5f * (settings.sizeMeters.z - 2.0f * radius);
            shape = new JPH::CapsuleShape(halfCylinder, radius);
            shapeRotation = JPH::Quat::sRotation(
                JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
        }
        else
        {
            shape = new JPH::BoxShape(JPH::Vec3(
                0.5f * settings.sizeMeters.x,
                0.5f * settings.sizeMeters.y,
                0.5f * settings.sizeMeters.z));
            if (settings.localPosition == Vec3{} &&
                settings.localRotationDegrees == Vec3{})
            {
                return shape;
            }
        }

        constexpr float radiansPerDegree = JPH::JPH_PI / 180.0f;
        const JPH::Quat authoredRotation = JPH::Quat::sEulerAngles(JPH::Vec3(
            settings.localRotationDegrees.x * radiansPerDegree,
            settings.localRotationDegrees.y * radiansPerDegree,
            settings.localRotationDegrees.z * radiansPerDegree));
        return new JPH::RotatedTranslatedShape(
            JPH::Vec3(settings.localPosition.x, settings.localPosition.y,
                settings.localPosition.z),
            authoredRotation * shapeRotation, shape);
    }
}
