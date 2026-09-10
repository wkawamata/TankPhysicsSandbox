#include "StaticMeshShape.h"

#include "PhysicsWorld.h"

#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <cmath>
#include <limits>

JPH_SUPPRESS_WARNINGS

namespace Tank::Physics
{
    namespace
    {
        bool IsFinite(const std::array<float, 3>& value)
        {
            return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
        }

        bool IsDegenerate(const Map::HitTriangleMesh& mesh, const std::array<uint32_t, 3>& triangle)
        {
            const auto& a = mesh.vertices[triangle[0]];
            const auto& b = mesh.vertices[triangle[1]];
            const auto& c = mesh.vertices[triangle[2]];
            const float abX = b[0] - a[0], abY = b[1] - a[1], abZ = b[2] - a[2];
            const float acX = c[0] - a[0], acY = c[1] - a[1], acZ = c[2] - a[2];
            const float crossX = abY * acZ - abZ * acY;
            const float crossY = abZ * acX - abX * acZ;
            const float crossZ = abX * acY - abY * acX;
            return crossX * crossX + crossY * crossY + crossZ * crossZ <= 1.0e-12f;
        }
    }

    bool CreateStaticMeshShape(
        const Map::HitTriangleMesh& mesh,
        JPH::ShapeRefC& output,
        std::string& error)
    {
        if (mesh.vertices.empty() || mesh.triangles.empty())
        {
            error = "HitMesh must contain vertices and triangles.";
            return false;
        }
        if (mesh.vertices.size() > std::numeric_limits<JPH::uint32>::max())
        {
            error = "HitMesh has too many vertices for Jolt.";
            return false;
        }

        JPH::VertexList vertices;
        vertices.reserve(mesh.vertices.size());
        for (const auto& vertex : mesh.vertices)
        {
            if (!IsFinite(vertex))
            {
                error = "HitMesh contains a non-finite vertex.";
                return false;
            }
            vertices.emplace_back(vertex[0], vertex[1], vertex[2]);
        }

        JPH::IndexedTriangleList triangles;
        triangles.reserve(mesh.triangles.size());
        for (size_t index = 0; index < mesh.triangles.size(); ++index)
        {
            const auto& triangle = mesh.triangles[index];
            if (triangle[0] >= mesh.vertices.size() || triangle[1] >= mesh.vertices.size() ||
                triangle[2] >= mesh.vertices.size())
            {
                error = "HitMesh triangle " + std::to_string(index) + " references a missing vertex.";
                return false;
            }
            if (triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[2] == triangle[0] ||
                IsDegenerate(mesh, triangle))
            {
                error = "HitMesh triangle " + std::to_string(index) + " is degenerate.";
                return false;
            }
            triangles.emplace_back(triangle[0], triangle[1], triangle[2]);
        }

        JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (!result.IsValid())
        {
            error = result.HasError() ? result.GetError().c_str() : "Jolt did not create a mesh shape.";
            return false;
        }
        output = result.Get();
        error.clear();
        return true;
    }

    bool AddStaticMeshBody(PhysicsWorld& world, const Map::HitTriangleMesh& mesh,
        JPH::BodyID& output, std::string& error)
    {
        JPH::ShapeRefC shape;
        if (!CreateStaticMeshShape(mesh, shape, error)) return false;
        JPH::BodyCreationSettings settings(
            shape.GetPtr(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0);
        const JPH::BodyID body = world.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (body.IsInvalid())
        {
            error = "Jolt could not create the static HitMesh body.";
            return false;
        }
        output = body;
        error.clear();
        return true;
    }
}
