#include "TankController.h"
#include "PhysicsWorld.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>

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
        float ClampNormalized(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }

    }

    struct TankController::Impl
    {
        PhysicsWorld& world;
        JPH::BodyID bodyId;
        JPH::Ref<JPH::VehicleConstraint> vehicleConstraint;
        bool hasBody = false;

        explicit Impl(PhysicsWorld& w) : world(w) {}

        ~Impl()
        {
            JPH::PhysicsSystem& physicsSystem = world.GetPhysicsSystem();

            if (vehicleConstraint != nullptr)
            {
                physicsSystem.RemoveStepListener(vehicleConstraint);
                physicsSystem.RemoveConstraint(vehicleConstraint);
                vehicleConstraint = nullptr;
            }

            if (hasBody)
            {
                JPH::BodyInterface& bodyInterface = world.GetBodyInterface();
                bodyInterface.RemoveBody(bodyId);
                bodyInterface.DestroyBody(bodyId);
                hasBody = false;
            }
        }
    };

    TankController::TankController() = default;
    TankController::~TankController() = default;

    void TankController::Initialize()
    {
        m_input = {};
        m_state = {};
        m_impl.reset();
    }

    void TankController::Initialize(PhysicsWorld& world)
    {
        m_input = {};
        m_state = {};
        m_impl = std::make_unique<Impl>(world);

        const float wheelRadius = 0.3f;
        const float wheelWidth = 0.1f;
        const float halfVehicleWidth = 1.0f;
        const float halfVehicleLength = 2.0f;
        const float halfVehicleHeight = 0.5f;
        const float suspensionMinLength = 0.3f;
        const float suspensionMaxLength = 0.5f;
        const float suspensionFrequency = 1.0f;

        JPH::BodyInterface& bodyInterface = world.GetBodyInterface();

        JPH::RefConst<JPH::Shape> tankBodyShape =
            JPH::OffsetCenterOfMassShapeSettings(
                JPH::Vec3(0, -halfVehicleHeight, 0),
                new JPH::BoxShape(JPH::Vec3(halfVehicleWidth, halfVehicleHeight, halfVehicleLength))
            ).Create().Get();

        JPH::BodyCreationSettings tankBodySettings(
            tankBodyShape,
            JPH::RVec3(0.0, 2.0, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::Moving);
        tankBodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        tankBodySettings.mMassPropertiesOverride.mMass = 4000.0f;

        JPH::Body* tankBody = bodyInterface.CreateBody(tankBodySettings);
        m_impl->bodyId = tankBody->GetID();
        m_impl->hasBody = true;
        bodyInterface.AddBody(m_impl->bodyId, JPH::EActivation::Activate);

        JPH::VehicleConstraintSettings vehicle;
        vehicle.mMaxPitchRollAngle = JPH::DegreesToRadians(60.0f);

        JPH::TrackedVehicleControllerSettings* controllerSettings =
            new JPH::TrackedVehicleControllerSettings;
        vehicle.mController = controllerSettings;

        for (int t = 0; t < 2; ++t)
        {
            JPH::VehicleTrackSettings& track = controllerSettings->mTracks[t];

            static const JPH::Vec3 wheelPos[] = {
                JPH::Vec3(0.0f, 0.0f, 2.0f),
                JPH::Vec3(0.0f, -0.3f, 1.0f),
                JPH::Vec3(0.0f, -0.3f, 0.0f),
                JPH::Vec3(0.0f, -0.3f, -1.0f),
                JPH::Vec3(0.0f, 0.0f, -2.0f),
            };

            constexpr int numWheels = static_cast<int>(sizeof(wheelPos) / sizeof(wheelPos[0]));
            track.mDrivenWheel = static_cast<JPH::uint>(vehicle.mWheels.size() + numWheels - 1);

            for (int w = 0; w < numWheels; ++w)
            {
                JPH::WheelSettingsTV* wheel = new JPH::WheelSettingsTV;
                wheel->mPosition = wheelPos[w];
                wheel->mPosition.SetX(t == 0 ? halfVehicleWidth : -halfVehicleWidth);
                wheel->mRadius = wheelRadius;
                wheel->mWidth = wheelWidth;
                wheel->mSuspensionMinLength = suspensionMinLength;
                wheel->mSuspensionMaxLength =
                    (w == 0 || w == numWheels - 1) ? suspensionMinLength : suspensionMaxLength;
                wheel->mSuspensionSpring.mFrequency = suspensionFrequency;

                track.mWheels.push_back(static_cast<JPH::uint>(vehicle.mWheels.size()));
                vehicle.mWheels.push_back(wheel);
            }
        }

        m_impl->vehicleConstraint = new JPH::VehicleConstraint(*tankBody, vehicle);
        m_impl->vehicleConstraint->SetVehicleCollisionTester(
            new JPH::VehicleCollisionTesterRay(Layers::Moving));

        JPH::PhysicsSystem& physicsSystem = world.GetPhysicsSystem();
        physicsSystem.AddConstraint(m_impl->vehicleConstraint);
        physicsSystem.AddStepListener(m_impl->vehicleConstraint);
    }

    void TankController::SetInput(const TankInput& input)
    {
        m_input.throttle = ClampNormalized(input.throttle);
        m_input.steering = ClampNormalized(input.steering);
        m_input.leftTrack = ClampNormalized(input.leftTrack);
        m_input.rightTrack = ClampNormalized(input.rightTrack);
        m_input.brake = input.brake;
    }

    void TankController::PreStep()
    {
        if (m_impl == nullptr)
        {
            return;
        }

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();
        bodyInterface.ActivateBody(m_impl->bodyId);

        float forward = m_input.throttle;
        float leftRatio = (m_input.leftTrack == 0.0f) ? 1.0f : m_input.leftTrack;
        float rightRatio = (m_input.rightTrack == 0.0f) ? 1.0f : m_input.rightTrack;
        float brake = m_input.brake ? 1.0f : 0.0f;

        JPH::TrackedVehicleController* controller =
            static_cast<JPH::TrackedVehicleController*>(
                m_impl->vehicleConstraint->GetController());
        controller->SetDriverInput(forward, leftRatio, rightRatio, brake);
    }

    void TankController::PostStep(float deltaTimeSeconds)
    {
        if (deltaTimeSeconds <= 0.0f)
        {
            return;
        }

        m_state.stepIndex++;
        m_state.timeSeconds += deltaTimeSeconds;

        if (m_impl == nullptr)
        {
            return;
        }

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();
        const JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(m_impl->bodyId);
        const JPH::Quat rotation = bodyInterface.GetRotation(m_impl->bodyId);
        const JPH::Vec3 linearVelocity = bodyInterface.GetLinearVelocity(m_impl->bodyId);
        const JPH::Vec3 angularVelocity = bodyInterface.GetAngularVelocity(m_impl->bodyId);

        m_state.body.position = {
            static_cast<float>(position.GetX()),
            static_cast<float>(position.GetY()),
            static_cast<float>(position.GetZ())};
        m_state.body.rotation = {
            static_cast<float>(rotation.GetX()),
            static_cast<float>(rotation.GetY()),
            static_cast<float>(rotation.GetZ()),
            static_cast<float>(rotation.GetW())};
        m_state.linearVelocity = {
            static_cast<float>(linearVelocity.GetX()),
            static_cast<float>(linearVelocity.GetY()),
            static_cast<float>(linearVelocity.GetZ())};
        m_state.angularVelocity = {
            static_cast<float>(angularVelocity.GetX()),
            static_cast<float>(angularVelocity.GetY()),
            static_cast<float>(angularVelocity.GetZ())};
        m_state.sleeping = !bodyInterface.IsActive(m_impl->bodyId);
    }
}
