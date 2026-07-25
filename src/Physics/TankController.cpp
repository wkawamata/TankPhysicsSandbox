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
#include <cmath>

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

        float ToJoltTrackRatio(float value)
        {
            constexpr float minimumMagnitude = 0.001f;
            if (std::abs(value) < minimumMagnitude)
            {
                return value < 0.0f ? -minimumMagnitude : minimumMagnitude;
            }
            return value;
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
        m_settings = {};
        m_impl.reset();
    }

    void TankController::Initialize(PhysicsWorld& world)
    {
        Initialize(world, {});
    }

    void TankController::Initialize(PhysicsWorld& world, const TankSettings& settings)
    {
        m_input = {};
        m_state = {};
        m_settings = settings;
        m_settings.chassisMassKg = (std::max)(m_settings.chassisMassKg, 1.0f);
        m_settings.rollTorqueNm = (std::max)(m_settings.rollTorqueNm, 0.0f);
        m_settings.rideHeightScale =
            std::clamp(m_settings.rideHeightScale, 0.7f, 0.9f);
        m_impl = std::make_unique<Impl>(world);

        const float wheelRadius = 0.3f;
        const float wheelWidth = 0.1f;
        const float halfVehicleWidth = 1.2f;
        const float halfVehicleLength = 2.0f;
        const float halfVehicleHeight = 0.5f;
        const float suspensionMinLength = 0.3f * m_settings.rideHeightScale;
        const float suspensionMaxLength = 0.5f * m_settings.rideHeightScale;
        const float suspensionFrequency = 1.0f;

        JPH::BodyInterface& bodyInterface = world.GetBodyInterface();

        JPH::RefConst<JPH::Shape> tankBodyShape =
            new JPH::BoxShape(JPH::Vec3(halfVehicleWidth, halfVehicleHeight, halfVehicleLength));
        const JPH::Quat initialRotation = m_settings.startUpsideDown
            ? JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI)
            : JPH::Quat::sIdentity();

        JPH::BodyCreationSettings tankBodySettings(
            tankBodyShape,
            JPH::RVec3(0.0, 2.0, 0.0),
            initialRotation,
            JPH::EMotionType::Dynamic,
            Layers::Moving);
        tankBodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        tankBodySettings.mMassPropertiesOverride.mMass = m_settings.chassisMassKg;

        JPH::Body* tankBody = bodyInterface.CreateBody(tankBodySettings);
        m_impl->bodyId = tankBody->GetID();
        m_impl->hasBody = true;
        bodyInterface.AddBody(m_impl->bodyId, JPH::EActivation::Activate);

        JPH::VehicleConstraintSettings vehicle;
        vehicle.mMaxPitchRollAngle = JPH::JPH_PI;

        JPH::TrackedVehicleControllerSettings* controllerSettings =
            new JPH::TrackedVehicleControllerSettings;
        vehicle.mController = controllerSettings;

        for (int t = 0; t < 2; ++t)
        {
            JPH::VehicleTrackSettings& track = controllerSettings->mTracks[t];

            static const JPH::Vec3 lowerWheelPos[] = {
                JPH::Vec3(0.0f, 0.0f, 2.0f),
                JPH::Vec3(0.0f, -0.3f, 1.0f),
                JPH::Vec3(0.0f, -0.3f, 0.0f),
                JPH::Vec3(0.0f, -0.3f, -1.0f),
                JPH::Vec3(0.0f, 0.0f, -2.0f),
            };

            constexpr int numWheelsPerSurface =
                static_cast<int>(sizeof(lowerWheelPos) / sizeof(lowerWheelPos[0]));
            track.mDrivenWheel =
                static_cast<JPH::uint>(vehicle.mWheels.size() + numWheelsPerSurface - 1);

            for (int surface = 0; surface < kTankSurfacesPerTrack; ++surface)
            {
                const bool upperSurface = surface == 1;
                for (int w = 0; w < numWheelsPerSurface; ++w)
                {
                    JPH::WheelSettingsTV* wheel = new JPH::WheelSettingsTV;
                    wheel->mPosition = lowerWheelPos[w];
                    wheel->mPosition.SetX(t == 0 ? halfVehicleWidth : -halfVehicleWidth);
                    if (upperSurface)
                    {
                        wheel->mPosition.SetY(-wheel->mPosition.GetY());
                        wheel->mSuspensionDirection = JPH::Vec3::sAxisY();
                        wheel->mSteeringAxis = -JPH::Vec3::sAxisY();
                        wheel->mWheelUp = -JPH::Vec3::sAxisY();
                    }
                    wheel->mRadius = wheelRadius;
                    wheel->mWidth = wheelWidth;
                    wheel->mSuspensionMinLength = suspensionMinLength;
                    wheel->mSuspensionMaxLength =
                        (w == 0 || w == numWheelsPerSurface - 1)
                        ? suspensionMinLength
                        : suspensionMaxLength;
                    wheel->mSuspensionSpring.mFrequency = suspensionFrequency;

                    track.mWheels.push_back(static_cast<JPH::uint>(vehicle.mWheels.size()));
                    vehicle.mWheels.push_back(wheel);
                }
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
        m_input.roll = ClampNormalized(input.roll);
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

        const JPH::Quat bodyRotation = bodyInterface.GetRotation(m_impl->bodyId);
        const JPH::Vec3 bodyUp = bodyRotation * JPH::Vec3::sAxisY();
        const JPH::Vec3 bodyForward = bodyRotation * JPH::Vec3::sAxisZ();
        if (m_input.roll != 0.0f)
        {
            bodyInterface.AddTorque(
                m_impl->bodyId,
                bodyForward * (m_input.roll * m_settings.rollTorqueNm));
        }
        else
        {
            constexpr float stabilizationTorque = 30000.0f;
            constexpr float stabilizationDamping = 10000.0f;
            const JPH::Vec3 targetUp =
                bodyUp.Dot(JPH::Vec3::sAxisY()) >= 0.0f
                ? JPH::Vec3::sAxisY()
                : -JPH::Vec3::sAxisY();
            const float rollError = bodyUp.Cross(targetUp).Dot(bodyForward);
            const float rollAngularVelocity =
                bodyInterface.GetAngularVelocity(m_impl->bodyId).Dot(bodyForward);
            bodyInterface.AddTorque(
                m_impl->bodyId,
                bodyForward *
                    (rollError * stabilizationTorque -
                        rollAngularVelocity * stabilizationDamping));
        }

        float forward = m_input.throttle;
        float leftRatio = ToJoltTrackRatio(m_input.leftTrack);
        float rightRatio = ToJoltTrackRatio(m_input.rightTrack);
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

        const auto& wheels = m_impl->vehicleConstraint->GetWheels();
        m_state.wheelCount = (std::min)(static_cast<int>(wheels.size()), kTankWheelCount);
        for (int i = 0; i < m_state.wheelCount; ++i)
        {
            const JPH::Wheel* wheel = wheels[static_cast<size_t>(i)];
            const JPH::RMat44 wheelTransform = m_impl->vehicleConstraint->GetWheelWorldTransform(
                static_cast<JPH::uint>(i),
                JPH::Vec3::sAxisY(),
                JPH::Vec3::sAxisX());
            const JPH::RVec3 wheelPosition = wheelTransform.GetTranslation();
            const JPH::Quat wheelRotation = wheelTransform.GetQuaternion();

            TrackedWheelState& wheelState = m_state.wheels[static_cast<size_t>(i)];
            wheelState.trackIndex = i / kTankWheelsPerTrack;
            wheelState.wheelIndex = i % kTankWheelsPerTrack;
            wheelState.upperSurface = wheelState.wheelIndex >= kTankWheelsPerSurface;
            wheelState.transform.position = {
                static_cast<float>(wheelPosition.GetX()),
                static_cast<float>(wheelPosition.GetY()),
                static_cast<float>(wheelPosition.GetZ())};
            wheelState.transform.rotation = {
                static_cast<float>(wheelRotation.GetX()),
                static_cast<float>(wheelRotation.GetY()),
                static_cast<float>(wheelRotation.GetZ()),
                static_cast<float>(wheelRotation.GetW())};
            wheelState.suspensionLength = wheel->GetSuspensionLength();
            wheelState.hasContact = wheel->HasContact();
        }
        m_state.sleeping = !bodyInterface.IsActive(m_impl->bodyId);
    }
}
