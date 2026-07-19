#include "TrackedVehicleTest.h"
#include "PhysicsWorld.h"
#include "TankController.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

JPH_SUPPRESS_WARNINGS

namespace Tank::Physics
{
    namespace Layers
    {
        constexpr JPH::ObjectLayer NonMoving = 0;
        constexpr JPH::ObjectLayer Moving = 1;
    }

    struct TrackedVehicleTest::Impl
    {
        PhysicsWorld world;
        TankController controller;
        JPH::BodyID floorBodyId;
        bool hasFloorBody = false;

        ~Impl()
        {
            if (hasFloorBody)
            {
                JPH::BodyInterface& bodyInterface = world.GetBodyInterface();
                bodyInterface.RemoveBody(floorBodyId);
                bodyInterface.DestroyBody(floorBodyId);
            }
        }
    };

    TrackedVehicleTest::TrackedVehicleTest() = default;
    TrackedVehicleTest::~TrackedVehicleTest() = default;

    void TrackedVehicleTest::Initialize()
    {
        m_state = {};

        m_impl = std::make_unique<Impl>();
        m_impl->world.Initialize();

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();

        JPH::BodyCreationSettings floorSettings(
            new JPH::BoxShape(JPH::Vec3(100.0f, 1.0f, 100.0f)),
            JPH::RVec3(0.0, -1.0, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            Layers::NonMoving);

        JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
        m_impl->floorBodyId = floorBody->GetID();
        m_impl->hasFloorBody = true;
        bodyInterface.AddBody(m_impl->floorBodyId, JPH::EActivation::DontActivate);

        m_impl->controller.Initialize(m_impl->world);
    }

    void TrackedVehicleTest::SetInput(const TankInput& input)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        m_impl->controller.SetInput(input);
    }

    TrackedVehicleTestState TrackedVehicleTest::Step(float deltaTimeSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        if (deltaTimeSeconds <= 0.0f)
        {
            return m_state;
        }

        m_impl->controller.PreStep();
        m_impl->world.Step(deltaTimeSeconds);
        m_impl->controller.PostStep(deltaTimeSeconds);

        m_state.stepIndex = m_impl->controller.State().stepIndex;
        m_state.timeSeconds = m_impl->controller.State().timeSeconds;
        m_state.bodyPosition = m_impl->controller.State().body.position;
        m_state.bodyRotation = m_impl->controller.State().body.rotation;
        m_state.linearVelocity = m_impl->controller.State().linearVelocity;
        m_state.angularVelocity = m_impl->controller.State().angularVelocity;
        m_state.sleeping = m_impl->controller.State().sleeping;

        return m_state;
    }
}
