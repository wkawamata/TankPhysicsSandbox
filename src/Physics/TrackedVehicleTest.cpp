#include "TrackedVehicleTest.h"
#include "PhysicsWorld.h"
#include "TankController.h"
#include "TestObstacleLayout.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <vector>

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
        std::vector<JPH::BodyID> obstacleBodyIds;
        bool hasFloorBody = false;

        ~Impl()
        {
            JPH::BodyInterface& bodyInterface = world.GetBodyInterface();
            for (const JPH::BodyID obstacleBodyId : obstacleBodyIds)
            {
                bodyInterface.RemoveBody(obstacleBodyId);
                bodyInterface.DestroyBody(obstacleBodyId);
            }
            if (hasFloorBody)
            {
                bodyInterface.RemoveBody(floorBodyId);
                bodyInterface.DestroyBody(floorBodyId);
            }
        }
    };

    TrackedVehicleTest::TrackedVehicleTest() = default;
    TrackedVehicleTest::~TrackedVehicleTest() = default;

    void TrackedVehicleTest::Initialize()
    {
        Initialize({});
    }

    void TrackedVehicleTest::Initialize(const TankSettings& settings)
    {
        Initialize(settings, {});
    }

    void TrackedVehicleTest::Initialize(
        const TankSettings& settings,
        const PhysicsEnvironmentSettings& environmentSettings)
    {
        m_state = {};

        m_impl = std::make_unique<Impl>();
        m_impl->world.Initialize();

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();

        const float floorSizeM =
            std::clamp(environmentSettings.floorSizeM, 20.0f, 1000.0f);
        const float floorFriction =
            std::clamp(environmentSettings.floorFriction, 0.0f, 2.0f);
        const float floorHalfExtent = 0.5f * floorSizeM;
        JPH::BodyCreationSettings floorSettings(
            new JPH::BoxShape(JPH::Vec3(floorHalfExtent, 1.0f, floorHalfExtent)),
            JPH::RVec3(0.0, -1.0, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            Layers::NonMoving);
        floorSettings.mFriction = floorFriction;

        JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
        m_impl->floorBodyId = floorBody->GetID();
        m_impl->hasFloorBody = true;
        bodyInterface.AddBody(m_impl->floorBodyId, JPH::EActivation::DontActivate);

        const std::vector<TestObstaclePlacement> obstacleLayout =
            GenerateTestObstacleLayout(environmentSettings);
        m_impl->obstacleBodyIds.reserve(obstacleLayout.size());
        for (const TestObstaclePlacement& obstacle : obstacleLayout)
        {
            JPH::BodyCreationSettings obstacleSettings(
                new JPH::BoxShape(JPH::Vec3(
                    0.5f * kPassengerCarWidthM,
                    0.5f * kPassengerCarHeightM,
                    0.5f * kPassengerCarLengthM)),
                JPH::RVec3(
                    obstacle.position.x,
                    obstacle.position.y,
                    obstacle.position.z),
                JPH::Quat::sRotation(JPH::Vec3::sAxisY(), obstacle.yawRadians),
                JPH::EMotionType::Static,
                Layers::NonMoving);
            obstacleSettings.mFriction = floorFriction;

            JPH::Body* obstacleBody = bodyInterface.CreateBody(obstacleSettings);
            if (obstacleBody != nullptr)
            {
                const JPH::BodyID obstacleBodyId = obstacleBody->GetID();
                m_impl->obstacleBodyIds.push_back(obstacleBodyId);
                bodyInterface.AddBody(obstacleBodyId, JPH::EActivation::DontActivate);
            }
        }

        m_impl->controller.Initialize(m_impl->world, settings);
    }

    const TankSettings& TrackedVehicleTest::Settings() const
    {
        static const TankSettings defaultSettings;
        return m_impl != nullptr ? m_impl->controller.Settings() : defaultSettings;
    }

    const TrackedDriverInput& TrackedVehicleTest::DriverInput() const
    {
        static const TrackedDriverInput defaultInput;
        return m_impl != nullptr ? m_impl->controller.DriverInput() : defaultInput;
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
        m_state.yawSpeedDegrees = m_impl->controller.State().yawSpeedDegrees;
        m_state.yawSpeedLimited = m_impl->controller.State().yawSpeedLimited;
        m_state.wheels = m_impl->controller.State().wheels;
        m_state.wheelCount = m_impl->controller.State().wheelCount;
        m_state.sleeping = m_impl->controller.State().sleeping;

        return m_state;
    }
}
