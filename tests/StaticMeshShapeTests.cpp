#include "Map/GltfHitMesh.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/StaticMeshShape.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <limits>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        Tank::Physics::PhysicsWorld world;
        world.Initialize();

        Tank::Map::HitTriangleMesh mesh;
        mesh.vertices = { { 0.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.0f } };
        mesh.triangles = { { 0, 2, 1 } };
        JPH::ShapeRefC shape;
        std::string error;
        Check(Tank::Physics::CreateStaticMeshShape(mesh, shape, error), "Create a Jolt static triangle mesh shape");
        Check(shape != nullptr && shape->GetSubType() == JPH::EShapeSubType::Mesh,
            "Created shape has Jolt mesh subtype");

        Tank::Map::HitTriangleMesh floor;
        floor.vertices = {
            { -5.0f, 0.0f, -5.0f }, { 5.0f, 0.0f, -5.0f },
            { 5.0f, 0.0f, 5.0f }, { -5.0f, 0.0f, 5.0f }
        };
        floor.triangles = { { 0, 3, 2 }, { 0, 2, 1 } };
        Tank::Map::Transform placement;
        placement.position = { 2.0f, 1.0f, 3.0f };
        placement.rotationDegrees = { 0.0f, 90.0f, 0.0f };
        Tank::Map::HitTriangleMesh placedFloor;
        Check(Tank::Map::TransformHitMesh(floor, placement, placedFloor, error),
            "Apply Manifest transform before creating the Jolt body");
        JPH::BodyID floorId;
        Check(Tank::Physics::AddStaticMeshBody(world, placedFloor, floorId, error) && !floorId.IsInvalid(),
            "Add the transformed HitMesh as a static PhysicsWorld body");

        JPH::BodyCreationSettings boxSettings(
            new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f)),
            JPH::RVec3(2.0, 5.0, 3.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1);
        JPH::BodyInterface& bodies = world.GetBodyInterface();
        const JPH::BodyID boxId = bodies.CreateAndAddBody(boxSettings, JPH::EActivation::Activate);
        Check(!boxId.IsInvalid(), "Create a dynamic contact-test box");
        for (int step = 0; step < 300; ++step) world.Step(1.0f / 60.0f);
        const float finalY = static_cast<float>(bodies.GetCenterOfMassPosition(boxId).GetY());
        Check(finalY >= 1.45f && finalY <= 1.65f,
            "Dynamic box settles on the Manifest-positioned HitMesh");
        bodies.RemoveBody(boxId);
        bodies.DestroyBody(boxId);
        bodies.RemoveBody(floorId);
        bodies.DestroyBody(floorId);

        JPH::ShapeRefC unchanged = shape;
        mesh.triangles[0][2] = 3;
        Check(!Tank::Physics::CreateStaticMeshShape(mesh, unchanged, error) && unchanged == shape,
            "Reject missing vertex references without changing output");

        mesh.triangles[0] = { 0, 0, 1 };
        Check(!Tank::Physics::CreateStaticMeshShape(mesh, unchanged, error) && unchanged == shape,
            "Reject triangles with repeated indices");

        mesh.triangles[0] = { 0, 1, 2 };
        mesh.vertices[2] = { 4.0f, 0.0f, 0.0f };
        Check(!Tank::Physics::CreateStaticMeshShape(mesh, unchanged, error),
            "Reject triangles with collinear vertices");

        mesh.vertices[2][0] = std::numeric_limits<float>::infinity();
        Check(!Tank::Physics::CreateStaticMeshShape(mesh, unchanged, error),
            "Reject non-finite vertices");
        std::cout << "Jolt static HitMesh shape tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
