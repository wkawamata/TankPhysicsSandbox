#include "TankJoltShape.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <cmath>

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
            const float radius = 0.5f * settings.sizeMeters.y;
            const float halfCylinder =
                0.5f * (settings.sizeMeters.z - 2.0f * radius);
            constexpr float tolerance = 0.0001f;
            if (std::abs(settings.sizeMeters.x - settings.sizeMeters.y) <=
                tolerance)
            {
                shape = new JPH::CapsuleShape(halfCylinder, radius);
                shapeRotation = JPH::Quat::sRotation(
                    JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
            }
            else
            {
                constexpr int ringSegments = 16;
                constexpr int capSegments = 4;
                const float radiusX = 0.5f * settings.sizeMeters.x;
                JPH::Array<JPH::Vec3> points;
                points.reserve(2 * ringSegments * (capSegments + 1));
                for (const float end : {-1.0f, 1.0f})
                {
                    for (int cap = 0; cap <= capSegments; ++cap)
                    {
                        const float latitude = 0.5f * JPH::JPH_PI *
                            static_cast<float>(cap) / capSegments;
                        const float radialScale = std::cos(latitude);
                        const float z = end * (halfCylinder +
                            radius * std::sin(latitude));
                        for (int ring = 0; ring < ringSegments; ++ring)
                        {
                            const float angle = 2.0f * JPH::JPH_PI *
                                static_cast<float>(ring) / ringSegments;
                            points.emplace_back(radiusX * radialScale *
                                    std::cos(angle),
                                radius * radialScale * std::sin(angle), z);
                        }
                    }
                }
                JPH::ShapeSettings::ShapeResult result =
                    JPH::ConvexHullShapeSettings(points).Create();
                if (result.HasError()) return nullptr;
                shape = result.Get();
            }
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
