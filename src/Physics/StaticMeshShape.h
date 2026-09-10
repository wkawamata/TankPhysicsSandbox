#pragma once

#include "Map/GltfHitMesh.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <string>

namespace Tank::Physics
{
    class PhysicsWorld;

    // Converts map-local Hit geometry to an immutable Jolt mesh shape.
    // Failure leaves output unchanged.
    bool CreateStaticMeshShape(
        const Map::HitTriangleMesh& mesh,
        JPH::ShapeRefC& output,
        std::string& error);

    // Adds an immutable, world-space mesh as a non-moving body at the identity transform.
    bool AddStaticMeshBody(PhysicsWorld& world, const Map::HitTriangleMesh& mesh,
        JPH::BodyID& output, std::string& error);
}
