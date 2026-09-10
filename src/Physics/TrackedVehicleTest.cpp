#include "TrackedVehicleTest.h"
#include "PhysicsWorld.h"
#include "TankController.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
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
        const PhysicsEnvironmentSettings& environmentSettings,
        const std::vector<MapPrimitive>& mapPrimitives,
        const MapSpawn& spawn)
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

        m_impl->obstacleBodyIds.reserve(mapPrimitives.size());
        for (const MapPrimitive& primitive : mapPrimitives)
        {
            JPH::RefConst<JPH::Shape> shape;
            if (primitive.type == MapPrimitiveType::Box)
            {
                shape = new JPH::BoxShape(JPH::Vec3(
                    0.5f * primitive.size.x,
                    0.5f * primitive.size.y,
                    0.5f * primitive.size.z));
            }
            else if (primitive.type == MapPrimitiveType::TriangularPrism)
            {
                const float halfX = 0.5f * primitive.size.x;
                const float halfY = 0.5f * primitive.size.y;
                const float halfZ = 0.5f * primitive.size.z;
                JPH::Array<JPH::Vec3> points = {
                    { -halfX, -halfY, -halfZ },
                    { halfX, -halfY, -halfZ },
                    { -halfX, -halfY, halfZ },
                    { halfX, -halfY, halfZ },
                    { -halfX, halfY, halfZ },
                    { halfX, halfY, halfZ } };
                JPH::ShapeSettings::ShapeResult shapeResult =
                    JPH::ConvexHullShapeSettings(points).Create();
                if (shapeResult.HasError())
                {
                    continue;
                }
                shape = shapeResult.Get();
            }
            else
            {
                const uint32_t sampleCount = primitive.heightFieldSampleCount;
                if (sampleCount < 2 ||
                    primitive.heightFieldHeights.size() !=
                        static_cast<size_t>(sampleCount) * sampleCount)
                {
                    continue;
                }
                const float halfSpan =
                    0.5f * primitive.heightFieldCellSizeM * static_cast<float>(sampleCount - 1);
                JPH::ShapeSettings::ShapeResult shapeResult =
                    JPH::HeightFieldShapeSettings(
                        primitive.heightFieldHeights.data(),
                        JPH::Vec3(-halfSpan, 0.0f, -halfSpan),
                        JPH::Vec3(
                            primitive.heightFieldCellSizeM,
                            1.0f,
                            primitive.heightFieldCellSizeM),
                        sampleCount).Create();
                if (shapeResult.HasError())
                {
                    continue;
                }
                shape = shapeResult.Get();
            }
            JPH::BodyCreationSettings obstacleSettings(
                shape,
                JPH::RVec3(
                    primitive.position.x,
                    primitive.position.y,
                    primitive.position.z),
                JPH::Quat::sRotation(JPH::Vec3::sAxisY(), primitive.yawRadians),
                JPH::EMotionType::Static,
                Layers::NonMoving);
            obstacleSettings.mFriction = std::clamp(primitive.friction, 0.0f, 2.0f);

            JPH::Body* obstacleBody = bodyInterface.CreateBody(obstacleSettings);
            if (obstacleBody != nullptr)
            {
                const JPH::BodyID obstacleBodyId = obstacleBody->GetID();
                m_impl->obstacleBodyIds.push_back(obstacleBodyId);
                bodyInterface.AddBody(obstacleBodyId, JPH::EActivation::DontActivate);
            }
        }

        m_impl->controller.Initialize(m_impl->world, settings, spawn);
    }

    const TankSettings& TrackedVehicleTest::Settings() const
    {
        static const TankSettings defaultSettings;
        return m_impl != nullptr ? m_impl->controller.Settings() : defaultSettings;
    }

    const TankInput& TrackedVehicleTest::Input() const
    {
        static const TankInput defaultInput;
        return m_impl != nullptr ? m_impl->controller.Input() : defaultInput;
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

    bool TrackedVehicleTest::ApplyConfiguredRecoil()
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        return m_impl->controller.ApplyConfiguredRecoil();
    }

    bool TrackedVehicleTest::ApplyRecoilImpulse(float impulseNewtonSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        return m_impl->controller.ApplyRecoilImpulse(impulseNewtonSeconds);
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
        m_state.speedMetersPerSecond =
            m_impl->controller.State().speedMetersPerSecond;
        m_state.maximumSpeedMetersPerSecond =
            m_impl->controller.State().maximumSpeedMetersPerSecond;
        m_state.zeroToTenTimeSeconds =
            m_impl->controller.State().zeroToTenTimeSeconds;
        m_state.engineRpm = m_impl->controller.State().engineRpm;
        m_state.transmissionGear = m_impl->controller.State().transmissionGear;
        m_state.clutchFriction = m_impl->controller.State().clutchFriction;
        m_state.yawSpeedDegrees = m_impl->controller.State().yawSpeedDegrees;
        m_state.yawSpeedLimited = m_impl->controller.State().yawSpeedLimited;
        m_state.wheels = m_impl->controller.State().wheels;
        m_state.wheelCount = m_impl->controller.State().wheelCount;
        m_state.sleeping = m_impl->controller.State().sleeping;
        m_state.motionObservation =
            m_impl->controller.State().motionObservation;
        m_state.mobility = m_impl->controller.State().mobility;
        m_state.rollingPhase = m_impl->controller.State().rollingPhase;
        m_state.lastRollingDecision =
            m_impl->controller.State().lastRollingDecision;
        m_state.rollingDecisionCount =
            m_impl->controller.State().rollingDecisionCount;
        m_state.rollingDecisionCommandSign =
            m_impl->controller.State().rollingDecisionCommandSign;
        m_state.rollingDecisionInputSign =
            m_impl->controller.State().rollingDecisionInputSign;
        m_state.lastRollingTraceEvent =
            m_impl->controller.State().lastRollingTraceEvent;
        m_state.rollingTraceSequence =
            m_impl->controller.State().rollingTraceSequence;
        m_state.rollingTraceRequestSign =
            m_impl->controller.State().rollingTraceRequestSign;
        m_state.rollingTraceCommandSign =
            m_impl->controller.State().rollingTraceCommandSign;
        m_state.rollingTraceInputSign =
            m_impl->controller.State().rollingTraceInputSign;
        m_state.specialMove = m_impl->controller.State().specialMove;
        m_state.mortarAim = m_impl->controller.State().mortarAim;
        m_state.trackInputSwapped =
            m_impl->controller.State().trackInputSwapped;
        m_state.rollChainAvailable =
            m_impl->controller.State().rollChainAvailable;

        return m_state;
    }
}
