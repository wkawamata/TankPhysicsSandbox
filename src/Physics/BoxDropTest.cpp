#include "BoxDropTest.h"
#include "PhysicsWorld.h"

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

    namespace
    {
        JPH::QuatArg ToJoltQuat(const JPH::Quat& rotation)
        {
            return rotation;
        }
    }

    struct BoxDropTest::Impl
    {
        PhysicsWorld world;
        JPH::BodyID floorBodyId;
        JPH::BodyID boxBodyId;
        bool hasFloorBody = false;
        bool hasBoxBody = false;

        ~Impl()
        {
            JPH::BodyInterface& bodyInterface = world.GetBodyInterface();
            if (hasBoxBody)
            {
                bodyInterface.RemoveBody(boxBodyId);
                bodyInterface.DestroyBody(boxBodyId);
            }

            if (hasFloorBody)
            {
                bodyInterface.RemoveBody(floorBodyId);
                bodyInterface.DestroyBody(floorBodyId);
            }
        }
    };

    BoxDropTest::BoxDropTest() = default;
    BoxDropTest::~BoxDropTest() = default;

    void BoxDropTest::Initialize(const BoxDropSettings& settings)
    {
        m_settings = settings;
        m_state = {};
        m_state.boxPosition = {0.0f, m_settings.initialHeight, 0.0f};
        m_state.boxRotation = {0.0f, 0.0f, 0.0f, 1.0f};

        m_impl = std::make_unique<Impl>();
        m_impl->world.Initialize();

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();

        JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
        floorShapeSettings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
        JPH::BodyCreationSettings floorSettings(
            floorShapeResult.Get().GetPtr(),
            JPH::RVec3(0.0, static_cast<double>(m_settings.floorTopY) - 1.0, 0.0),
            ToJoltQuat(JPH::Quat::sIdentity()),
            JPH::EMotionType::Static,
            Layers::NonMoving);

        JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
        m_impl->floorBodyId = floorBody->GetID();
        m_impl->hasFloorBody = true;
        bodyInterface.AddBody(m_impl->floorBodyId, JPH::EActivation::DontActivate);

        JPH::BodyCreationSettings boxSettings(
            new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f)),
            JPH::RVec3(0.0, static_cast<double>(m_settings.initialHeight), 0.0),
            ToJoltQuat(JPH::Quat::sIdentity()),
            JPH::EMotionType::Dynamic,
            Layers::Moving);

        m_impl->boxBodyId = bodyInterface.CreateAndAddBody(boxSettings, JPH::EActivation::Activate);
        m_impl->hasBoxBody = true;
    }

    BoxDropState BoxDropTest::Step(float deltaTimeSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize(m_settings);
        }

        m_impl->world.Step(deltaTimeSeconds);

        m_state.stepIndex++;
        m_state.timeSeconds += deltaTimeSeconds;

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();
        const JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(m_impl->boxBodyId);
        const JPH::Quat rotation = bodyInterface.GetRotation(m_impl->boxBodyId);
        m_state.boxPosition = {
            static_cast<float>(position.GetX()),
            static_cast<float>(position.GetY()),
            static_cast<float>(position.GetZ())};
        m_state.boxRotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()};
        m_state.boxSleeping = !bodyInterface.IsActive(m_impl->boxBodyId);

        return m_state;
    }
}
