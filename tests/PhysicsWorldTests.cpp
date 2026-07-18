#include "Physics/PhysicsWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cmath>
#include <iostream>

JPH_SUPPRESS_WARNINGS

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL PhysicsWorld: " << message << "\n";
        }

        return condition;
    }
}

int main()
{
    bool passed = true;

    Tank::Physics::PhysicsWorld world;
    world.Initialize();

    JPH::BodyInterface& bodyInterface = world.GetBodyInterface();

    JPH::BodyCreationSettings floorSettings(
        new JPH::BoxShape(JPH::Vec3(100.0f, 1.0f, 100.0f)),
        JPH::RVec3(0.0, -1.0, 0.0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        0);

    JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
    passed &= Check(floorBody != nullptr, "floor body must be created");
    JPH::BodyID floorId = floorBody->GetID();
    bodyInterface.AddBody(floorId, JPH::EActivation::DontActivate);

    JPH::BodyCreationSettings boxSettings(
        new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f)),
        JPH::RVec3(0.0, 5.0, 0.0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        1);

    JPH::BodyID boxId = bodyInterface.CreateAndAddBody(boxSettings, JPH::EActivation::Activate);
    passed &= Check(!boxId.IsInvalid(), "box body must be created");

    JPH::RVec3 initialPos = bodyInterface.GetCenterOfMassPosition(boxId);
    passed &= Check(std::abs(static_cast<float>(initialPos.GetY()) - 5.0f) < 0.01f, "box initial Y must be near 5.0");

    for (int i = 0; i < 300; i++)
    {
        world.Step(1.0f / 60.0f);
    }

    JPH::RVec3 finalPos = bodyInterface.GetCenterOfMassPosition(boxId);
    float finalY = static_cast<float>(finalPos.GetY());
    passed &= Check(finalY >= 0.45f && finalY <= 0.65f, "box must settle near floor");

    bodyInterface.RemoveBody(boxId);
    bodyInterface.DestroyBody(boxId);
    bodyInterface.RemoveBody(floorId);
    bodyInterface.DestroyBody(floorId);

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS PhysicsWorld\n";
    return 0;
}
