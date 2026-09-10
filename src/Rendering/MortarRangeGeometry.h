#pragma once

#include "Physics/PhysicsTypes.h"
#include <cmath>
#include <vector>

namespace Tank::Rendering
{
    struct MortarRangeGeometry
    {
        static std::vector<Physics::Vec3> BuildCircle(
            const Physics::Vec3& center, float radius, int segments = 32)
        {
            std::vector<Physics::Vec3> vertices;
            if (!std::isfinite(radius) || radius <= 0.0f || segments < 3)
                return vertices;
            vertices.reserve(static_cast<size_t>(segments));
            constexpr float pi = 3.14159265358979323846f;
            for (int i = 0; i < segments; ++i)
            {
                const float angle = 2.0f * pi * static_cast<float>(i) /
                    static_cast<float>(segments);
                vertices.push_back({center.x + std::cos(angle) * radius,
                    center.y, center.z + std::sin(angle) * radius});
            }
            return vertices;
        }
    };
}
