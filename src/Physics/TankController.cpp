#include "TankController.h"

#include "TankMotionObservation.h"
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
        enum class RollingPhase
        {
            None,
            PoweredRoll,
            BallisticRoll,
            Settling
        };

        PhysicsWorld& world;
        JPH::BodyID bodyId;
        JPH::Ref<JPH::VehicleConstraint> vehicleConstraint;
        bool hasBody = false;
        bool rollInputLatched = false;
        RollingPhase rollingPhase = RollingPhase::None;
        int rollSettledFrames = 0;
        float rollDistanceIntegral = 0.0f;
        JPH::RVec3 rollStartPosition;
        JPH::Vec3 rollStartUp;
        JPH::Vec3 rollDirection;
        bool accelerationTiming = false;
        float accelerationStartTime = 0.0f;

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
        Initialize(world, settings, {});
    }

    void TankController::Initialize(
        PhysicsWorld& world,
        const TankSettings& settings,
        const MapSpawn& spawn)
    {
        m_input = {};
        m_state = {};
        m_settings = settings;
        m_mobilityStateMachine = MobilityStateMachine(m_settings);
        m_settings.chassisMassKg = (std::max)(m_settings.chassisMassKg, 1.0f);
        m_settings.rollTorqueNm = (std::max)(m_settings.rollTorqueNm, 0.0f);
        m_settings.rollDistanceM = std::clamp(m_settings.rollDistanceM, 0.5f, 5.0f);
        m_settings.rollTorqueCutoffDegrees =
            std::clamp(m_settings.rollTorqueCutoffDegrees, 45.0f, 120.0f);
        m_settings.rollStabilizationTorqueNm =
            (std::max)(m_settings.rollStabilizationTorqueNm, 0.0f);
        m_settings.rollStabilizationDampingNms =
            (std::max)(m_settings.rollStabilizationDampingNms, 0.0f);
        m_settings.trackWidthM = std::clamp(m_settings.trackWidthM, 0.15f, 1.0f);
        m_settings.trackSpacingM = std::clamp(m_settings.trackSpacingM, 1.8f, 6.0f);
        m_settings.trackLongitudinalFriction =
            std::clamp(m_settings.trackLongitudinalFriction, 0.0f, 10.0f);
        m_settings.trackLateralFriction =
            std::clamp(m_settings.trackLateralFriction, 0.0f, 10.0f);
        m_settings.chassisWidthM = std::clamp(m_settings.chassisWidthM, 1.6f, 3.2f);
        m_settings.chassisLengthM = std::clamp(m_settings.chassisLengthM, 3.0f, 5.5f);
        m_settings.endWheelRadiusM =
            std::clamp(m_settings.endWheelRadiusM, 0.01f, 0.6f);
        m_settings.roadWheelRadiusM =
            std::clamp(m_settings.roadWheelRadiusM, 0.2f, 0.5f);
        m_settings.clutchReleaseTimeSeconds =
            std::clamp(m_settings.clutchReleaseTimeSeconds, 0.01f, 0.5f);
        m_settings.engineMaxTorqueNm =
            std::clamp(m_settings.engineMaxTorqueNm, 100.0f, 5000.0f);
        m_settings.engineMaxRpm =
            std::clamp(m_settings.engineMaxRpm, 2000.0f, 10000.0f);
        m_settings.transmissionShiftDownRpm = std::clamp(
            m_settings.transmissionShiftDownRpm,
            500.0f,
            m_settings.engineMaxRpm - 200.0f);
        m_settings.transmissionShiftUpRpm = std::clamp(
            m_settings.transmissionShiftUpRpm,
            m_settings.transmissionShiftDownRpm + 100.0f,
            m_settings.engineMaxRpm - 100.0f);
        m_settings.transmissionClutchStrength = std::clamp(
            m_settings.transmissionClutchStrength,
            1.0f,
            100.0f);
        m_settings.finalDriveRatio =
            std::clamp(m_settings.finalDriveRatio, 0.25f, 4.0f);
        m_settings.roadWheelCount = std::clamp(m_settings.roadWheelCount, 2, 4);
        const float maximumEndWheelOffset =
            (std::max)(
                0.0f,
                0.5f * m_settings.chassisLengthM -
                    m_settings.endWheelRadiusM - 0.1f);
        m_settings.endWheelOffsetM = std::clamp(
            m_settings.endWheelOffsetM,
            0.0f,
            (std::min)(1.0f, maximumEndWheelOffset));
        const float endWheelPosition =
            0.5f * m_settings.chassisLengthM - m_settings.endWheelOffsetM;
        const float maximumTwoRoadWheelOffset =
            (std::max)(0.1f, endWheelPosition - m_settings.roadWheelRadiusM);
        m_settings.twoRoadWheelOffsetM = std::clamp(
            m_settings.twoRoadWheelOffsetM,
            0.1f,
            (std::min)(2.0f, maximumTwoRoadWheelOffset));
        m_settings.threeRoadWheelOffsetM = std::clamp(
            m_settings.threeRoadWheelOffsetM,
            0.1f,
            (std::min)(2.0f, maximumTwoRoadWheelOffset));
        m_settings.rideHeightScale =
            std::clamp(m_settings.rideHeightScale, 0.5f, 1.1f);
        m_settings.suspensionFrequencyHz =
            std::clamp(m_settings.suspensionFrequencyHz, 0.1f, 10.0f);
        m_settings.suspensionDamping =
            std::clamp(m_settings.suspensionDamping, 0.0f, 2.0f);
        for (float& stroke : m_settings.suspensionStrokeMeters)
        {
            stroke = std::clamp(stroke, 0.0f, 0.5f);
        }
        m_settings.wheelHorizontalOffsetM = std::clamp(
            m_settings.wheelHorizontalOffsetM,
            -1.0f,
            1.0f);
        m_impl = std::make_unique<Impl>(world);

        const float roadWheelRadius = m_settings.roadWheelRadiusM;
        const float wheelWidth = m_settings.trackWidthM;
        const float halfVehicleWidth = 0.5f * m_settings.chassisWidthM;
        const float halfTrackSpacing = 0.5f * m_settings.trackSpacingM;
        const float halfVehicleLength = 0.5f * m_settings.chassisLengthM;
        const float halfVehicleHeight = 0.5f;
        const float suspensionMinLength = 0.3f * m_settings.rideHeightScale;

        JPH::BodyInterface& bodyInterface = world.GetBodyInterface();

        JPH::RefConst<JPH::Shape> tankBodyShape =
            new JPH::BoxShape(JPH::Vec3(halfVehicleWidth, halfVehicleHeight, halfVehicleLength));
        const JPH::Quat yawRotation =
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(), spawn.yawRadians);
        const JPH::Quat initialRotation = m_settings.startUpsideDown ?
            yawRotation * JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI) :
            yawRotation;

        JPH::BodyCreationSettings tankBodySettings(
            tankBodyShape,
            JPH::RVec3(
                spawn.position.x,
                spawn.position.y,
                spawn.position.z),
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
        controllerSettings->mEngine.mMaxTorque = m_settings.engineMaxTorqueNm;
        controllerSettings->mEngine.mMaxRPM = m_settings.engineMaxRpm;
        controllerSettings->mTransmission.mShiftDownRPM =
            m_settings.transmissionShiftDownRpm;
        controllerSettings->mTransmission.mShiftUpRPM =
            m_settings.transmissionShiftUpRpm;
        controllerSettings->mTransmission.mClutchStrength =
            m_settings.transmissionClutchStrength;
        for (float& ratio : controllerSettings->mTransmission.mGearRatios)
        {
            ratio *= m_settings.finalDriveRatio;
        }
        for (float& ratio : controllerSettings->mTransmission.mReverseGearRatios)
        {
            ratio *= m_settings.finalDriveRatio;
        }
        controllerSettings->mTransmission.mClutchReleaseTime =
            std::clamp(m_settings.clutchReleaseTimeSeconds, 0.01f, 0.5f);
        vehicle.mController = controllerSettings;

        for (int t = 0; t < 2; ++t)
        {
            JPH::VehicleTrackSettings& track = controllerSettings->mTracks[t];

            const int numWheelsPerSurface = m_settings.roadWheelCount + 2;
            track.mDrivenWheel =
                static_cast<JPH::uint>(vehicle.mWheels.size() + numWheelsPerSurface - 1);

            for (int surface = 0; surface < kTankSurfacesPerTrack; ++surface)
            {
                const bool upperSurface = surface == 1;
                for (int w = 0; w < numWheelsPerSurface; ++w)
                {
                    JPH::WheelSettingsTV* wheel = new JPH::WheelSettingsTV;
                    wheel->mLongitudinalFriction =
                        m_settings.trackLongitudinalFriction;
                    wheel->mLateralFriction = m_settings.trackLateralFriction;
                    const float wheelFraction =
                        static_cast<float>(w) / static_cast<float>(numWheelsPerSurface - 1);
                    const bool endWheel = w == 0 || w == numWheelsPerSurface - 1;
                    float wheelZ =
                        halfVehicleLength - wheelFraction * m_settings.chassisLengthM;
                    if (endWheel)
                    {
                        wheelZ = w == 0 ? endWheelPosition : -endWheelPosition;
                    }
                    if (m_settings.roadWheelCount == 2 && !endWheel)
                    {
                        wheelZ = w == 1
                            ? m_settings.twoRoadWheelOffsetM
                            : -m_settings.twoRoadWheelOffsetM;
                    }
                    else if (m_settings.roadWheelCount == 3 && !endWheel)
                    {
                        wheelZ = w == 1
                            ? m_settings.threeRoadWheelOffsetM
                            : (w == 2 ? 0.0f : -m_settings.threeRoadWheelOffsetM);
                    }
                    wheel->mPosition =
                        JPH::Vec3(
                            0.0f,
                            endWheel ? m_settings.endWheelVerticalOffsetM
                                     : -roadWheelRadius + m_settings.roadWheelVerticalOffsetM,
                            wheelZ);
                    const float wheelHorizontalPosition =
                        halfTrackSpacing + m_settings.wheelHorizontalOffsetM;
                    wheel->mPosition.SetX(
                        t == 0 ? wheelHorizontalPosition : -wheelHorizontalPosition);
                    if (upperSurface)
                    {
                        wheel->mPosition.SetY(-wheel->mPosition.GetY());
                        wheel->mSuspensionDirection = JPH::Vec3::sAxisY();
                        wheel->mSteeringAxis = -JPH::Vec3::sAxisY();
                        wheel->mWheelUp = -JPH::Vec3::sAxisY();
                    }
                    wheel->mRadius = endWheel
                        ? m_settings.endWheelRadiusM
                        : m_settings.roadWheelRadiusM;
                    wheel->mWidth = wheelWidth;
                    wheel->mSuspensionForcePoint = wheel->mPosition;
                    wheel->mEnableSuspensionForcePoint = true;
                    wheel->mSuspensionMinLength = suspensionMinLength;
                    const int suspensionPosition = w == 0
                        ? 0
                        : (w == numWheelsPerSurface - 1
                            ? kTankSuspensionPositionsPerSurface - 1
                            : w);
                    const int suspensionSlot = TankSuspensionSlotIndex(
                        t, surface, suspensionPosition);
                    wheel->mSuspensionMaxLength = suspensionMinLength +
                        m_settings.suspensionStrokeMeters[
                            static_cast<size_t>(suspensionSlot)];
                    wheel->mSuspensionSpring.mFrequency =
                        m_settings.suspensionFrequencyHz;
                    wheel->mSuspensionSpring.mDamping =
                        m_settings.suspensionDamping;

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
        m_input.roll =
            m_settings.rollingInputEnabled ? ClampNormalized(input.roll) : 0.0f;
        m_input.brakeAmount = std::clamp(input.brakeAmount, 0.0f, 1.0f);
        m_input.brake = input.brake;
    }

    bool TankController::ApplyConfiguredRecoil()
    {
        if (m_impl == nullptr ||
            !std::isfinite(m_settings.recoilImpulseNewtonSeconds) ||
            m_settings.recoilImpulseNewtonSeconds <= 0.0f ||
            !std::isfinite(m_settings.recoilPointForwardM) ||
            !std::isfinite(m_settings.recoilPointHeightM))
        {
            return false;
        }

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();
        const JPH::Quat bodyRotation = bodyInterface.GetRotation(m_impl->bodyId);
        const JPH::Vec3 bodyForward = bodyRotation * JPH::Vec3::sAxisZ();
        const JPH::Vec3 localPoint(
            0.0f,
            m_settings.recoilPointHeightM,
            m_settings.recoilPointForwardM);
        const JPH::RVec3 worldPoint =
            bodyInterface.GetCenterOfMassPosition(m_impl->bodyId) +
            bodyRotation * localPoint;
        bodyInterface.AddImpulse(
            m_impl->bodyId,
            -m_settings.recoilImpulseNewtonSeconds * bodyForward,
            worldPoint);
        return true;
    }

    bool TankController::ApplyRecoilImpulse(float impulseNewtonSeconds)
    {
        if (m_impl == nullptr || !std::isfinite(impulseNewtonSeconds) ||
            impulseNewtonSeconds <= 0.0f)
        {
            return false;
        }

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();
        const JPH::Quat bodyRotation = bodyInterface.GetRotation(m_impl->bodyId);
        const JPH::Vec3 bodyForward = bodyRotation * JPH::Vec3::sAxisZ();
        bodyInterface.AddImpulse(m_impl->bodyId, -impulseNewtonSeconds * bodyForward);
        return true;
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
        const bool hasRollInput = m_input.roll != 0.0f;
        if (hasRollInput && !m_impl->rollInputLatched)
        {
            const JPH::Vec3 bodyRight = bodyRotation * JPH::Vec3::sAxisX();
            m_impl->rollInputLatched = true;
            m_impl->rollingPhase = Impl::RollingPhase::PoweredRoll;
            m_impl->rollSettledFrames = 0;
            m_impl->rollDistanceIntegral = 0.0f;
            m_impl->rollStartPosition =
                bodyInterface.GetCenterOfMassPosition(m_impl->bodyId);
            m_impl->rollStartUp = bodyUp;
            m_impl->rollDirection =
                m_input.roll > 0.0f ? -bodyRight : bodyRight;
        }
        else if (!hasRollInput)
        {
            m_impl->rollInputLatched = false;
        }

        if (m_impl->rollingPhase == Impl::RollingPhase::PoweredRoll)
        {
            const float cutoffDot = std::cos(
                JPH::DegreesToRadians(m_settings.rollTorqueCutoffDegrees));
            if (bodyUp.Dot(m_impl->rollStartUp) > cutoffDot && hasRollInput)
            {
                bodyInterface.AddTorque(
                    m_impl->bodyId,
                    bodyForward * (m_input.roll * m_settings.rollTorqueNm));
            }
            else
            {
                m_impl->rollingPhase = Impl::RollingPhase::BallisticRoll;
            }
        }

        if (m_impl->rollingPhase == Impl::RollingPhase::BallisticRoll ||
            m_impl->rollingPhase == Impl::RollingPhase::Settling)
        {
            constexpr float positionGain = 80000.0f;
            constexpr float integralGain = 40000.0f;
            constexpr float velocityGain = 25000.0f;
            constexpr float maximumForce = 200000.0f;
            const JPH::RVec3 position =
                bodyInterface.GetCenterOfMassPosition(m_impl->bodyId);
            const float lateralDistance = static_cast<float>(
                (position - m_impl->rollStartPosition).Dot(m_impl->rollDirection));
            const float lateralVelocity =
                bodyInterface.GetLinearVelocity(m_impl->bodyId).Dot(m_impl->rollDirection);
            const float rollAngularVelocity =
                bodyInterface.GetAngularVelocity(m_impl->bodyId).Dot(bodyForward);
            const float distanceError = m_settings.rollDistanceM - lateralDistance;
            m_impl->rollDistanceIntegral = std::clamp(
                m_impl->rollDistanceIntegral + distanceError / 60.0f,
                -2.0f,
                2.0f);
            const float force = std::clamp(
                distanceError * positionGain +
                    m_impl->rollDistanceIntegral * integralGain -
                    lateralVelocity * velocityGain,
                -maximumForce,
                maximumForce);
            bodyInterface.AddForce(m_impl->bodyId, m_impl->rollDirection * force);

            if (m_impl->rollingPhase == Impl::RollingPhase::BallisticRoll &&
                std::abs(distanceError) < 0.05f &&
                std::abs(lateralVelocity) < 0.1f &&
                std::abs(bodyUp.Dot(JPH::Vec3::sAxisY())) > 0.95f &&
                std::abs(rollAngularVelocity) < 0.1f)
            {
                ++m_impl->rollSettledFrames;
                if (m_impl->rollSettledFrames >= 60)
                {
                    m_impl->rollingPhase = Impl::RollingPhase::None;
                }
            }
            else
            {
                m_impl->rollSettledFrames = 0;
            }
        }

        if (m_impl->rollingPhase != Impl::RollingPhase::PoweredRoll)
        {
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
                    (rollError * m_settings.rollStabilizationTorqueNm -
                        rollAngularVelocity * m_settings.rollStabilizationDampingNms));
        }

        float forward = m_input.throttle;
        float leftTrack = m_input.leftTrack;
        float rightTrack = m_input.rightTrack;
        constexpr float turnInputEpsilon = 0.001f;
        const bool stationaryTurn =
            (std::abs(leftTrack) < turnInputEpsilon) !=
            (std::abs(rightTrack) < turnInputEpsilon);
        const bool pivotTurn = leftTrack * rightTrack < 0.0f;
        if (stationaryTurn)
        {
            const float innerTrackRatio =
                std::clamp(m_settings.stationaryTurnInnerTrackRatio, 0.0f, 1.0f);
            if (std::abs(leftTrack) < turnInputEpsilon)
            {
                leftTrack = std::copysign(innerTrackRatio, rightTrack);
            }
            else
            {
                rightTrack = std::copysign(innerTrackRatio, leftTrack);
            }
            leftTrack *= std::clamp(m_settings.stationaryTurnLeftTraction, 0.0f, 1.0f);
            rightTrack *= std::clamp(m_settings.stationaryTurnRightTraction, 0.0f, 1.0f);
        }
        else if (pivotTurn)
        {
            leftTrack *= std::clamp(m_settings.pivotTurnLeftTraction, 0.0f, 1.0f);
            rightTrack *= std::clamp(m_settings.pivotTurnRightTraction, 0.0f, 1.0f);
        }
        float leftRatio = ToJoltTrackRatio(leftTrack);
        float rightRatio = ToJoltTrackRatio(rightTrack);
        float brake = m_input.brake ? 1.0f : m_input.brakeAmount;
        m_driverInput = {forward, leftRatio, rightRatio, brake};

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
        JPH::Vec3 angularVelocity = bodyInterface.GetAngularVelocity(m_impl->bodyId);
        const JPH::Vec3 bodyUp = rotation * JPH::Vec3::sAxisY();
        const float yawVelocity = angularVelocity.Dot(bodyUp);
        const float yawSpeedLimit = std::clamp(
            m_settings.yawSpeedLimitDegrees,
            15.0f,
            720.0f) * JPH::JPH_PI / 180.0f;
        m_state.yawSpeedLimited = std::abs(yawVelocity) > yawSpeedLimit;
        float limitedYawVelocity =
            std::clamp(yawVelocity, -yawSpeedLimit, yawSpeedLimit);
        constexpr float turnInputEpsilon = 0.001f;
        if (std::abs(m_input.leftTrack - m_input.rightTrack) <
            turnInputEpsilon)
        {
            const float yawDamping = std::clamp(
                m_settings.yawDamping,
                0.0f,
                30.0f);
            limitedYawVelocity *= std::exp(-yawDamping * deltaTimeSeconds);
        }
        m_state.yawSpeedDegrees =
            limitedYawVelocity * 180.0f / JPH::JPH_PI;
        angularVelocity += bodyUp * (limitedYawVelocity - yawVelocity);
        bodyInterface.SetAngularVelocity(m_impl->bodyId, angularVelocity);

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
        m_state.speedMetersPerSecond = std::sqrt(
            m_state.linearVelocity.x * m_state.linearVelocity.x +
            m_state.linearVelocity.z * m_state.linearVelocity.z);
        m_state.maximumSpeedMetersPerSecond = (std::max)(
            m_state.maximumSpeedMetersPerSecond,
            m_state.speedMetersPerSecond);

        const bool driveRequested = std::abs(m_input.throttle) > 0.001f;
        if (!driveRequested && m_state.speedMetersPerSecond < 0.1f)
        {
            m_impl->accelerationTiming = false;
            m_state.zeroToTenTimeSeconds = -1.0f;
        }
        else if (driveRequested && !m_impl->accelerationTiming &&
            m_state.zeroToTenTimeSeconds < 0.0f)
        {
            m_impl->accelerationTiming = true;
            m_impl->accelerationStartTime = m_state.timeSeconds;
        }
        if (m_impl->accelerationTiming && m_state.speedMetersPerSecond >= 10.0f)
        {
            m_state.zeroToTenTimeSeconds =
                m_state.timeSeconds - m_impl->accelerationStartTime;
            m_impl->accelerationTiming = false;
        }

        const JPH::TrackedVehicleController* controller =
            static_cast<const JPH::TrackedVehicleController*>(
                m_impl->vehicleConstraint->GetController());
        m_state.engineRpm = controller->GetEngine().GetCurrentRPM();
        m_state.transmissionGear = controller->GetTransmission().GetCurrentGear();
        m_state.clutchFriction = controller->GetTransmission().GetClutchFriction();
        m_state.angularVelocity = {
            static_cast<float>(angularVelocity.GetX()),
            static_cast<float>(angularVelocity.GetY()),
            static_cast<float>(angularVelocity.GetZ())};

        const auto& wheels = m_impl->vehicleConstraint->GetWheels();
        const JPH::RMat44 bodyTransform =
            bodyInterface.GetWorldTransform(m_impl->bodyId);
        const int wheelsPerSurface = m_settings.roadWheelCount + 2;
        const int wheelsPerTrack = wheelsPerSurface * kTankSurfacesPerTrack;
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
            const JPH::WheelSettings* wheelSettings = wheel->GetSettings();
            const JPH::RVec3 suspensionOrigin =
                bodyTransform * wheelSettings->mPosition;
            const JPH::Vec3 suspensionDirection =
                bodyTransform.Multiply3x3(wheelSettings->mSuspensionDirection);

            TrackedWheelState& wheelState = m_state.wheels[static_cast<size_t>(i)];
            wheelState.trackIndex = i / wheelsPerTrack;
            wheelState.wheelIndex = i % wheelsPerTrack;
            wheelState.upperSurface = wheelState.wheelIndex >= wheelsPerSurface;
            const int wheelOnSurface = wheelState.wheelIndex % wheelsPerSurface;
            const int suspensionPosition = wheelOnSurface == 0
                ? 0
                : (wheelOnSurface == wheelsPerSurface - 1
                    ? kTankSuspensionPositionsPerSurface - 1
                    : wheelOnSurface);
            wheelState.suspensionSlotIndex = TankSuspensionSlotIndex(
                wheelState.trackIndex,
                wheelState.upperSurface ? 1 : 0,
                suspensionPosition);
            wheelState.transform.position = {
                static_cast<float>(wheelPosition.GetX()),
                static_cast<float>(wheelPosition.GetY()),
                static_cast<float>(wheelPosition.GetZ())};
            wheelState.transform.rotation = {
                static_cast<float>(wheelRotation.GetX()),
                static_cast<float>(wheelRotation.GetY()),
                static_cast<float>(wheelRotation.GetZ()),
                static_cast<float>(wheelRotation.GetW())};
            wheelState.suspensionOrigin = {
                static_cast<float>(suspensionOrigin.GetX()),
                static_cast<float>(suspensionOrigin.GetY()),
                static_cast<float>(suspensionOrigin.GetZ())};
            wheelState.suspensionDirection = {
                static_cast<float>(suspensionDirection.GetX()),
                static_cast<float>(suspensionDirection.GetY()),
                static_cast<float>(suspensionDirection.GetZ())};
            const float previousSuspensionLength = wheelState.suspensionLength;
            wheelState.suspensionLength = wheel->GetSuspensionLength();
            wheelState.suspensionVelocityMetersPerSecond = m_state.stepIndex > 1
                ? (wheelState.suspensionLength - previousSuspensionLength) /
                    deltaTimeSeconds
                : 0.0f;
            wheelState.suspensionMinLength = wheelSettings->mSuspensionMinLength;
            wheelState.suspensionMaxLength = wheelSettings->mSuspensionMaxLength;
            wheelState.suspensionAtHardPoint = wheel->HasHitHardPoint();
            wheelState.angularVelocityRadians = wheel->GetAngularVelocity();
            wheelState.suspensionImpulseNewtonSeconds = wheel->GetSuspensionLambda();
            wheelState.longitudinalImpulseNewtonSeconds =
                wheel->GetLongitudinalLambda();
            wheelState.lateralImpulseNewtonSeconds = wheel->GetLateralLambda();
            wheelState.longitudinalSlipMetersPerSecond = 0.0f;
            wheelState.hasContact = wheel->HasContact();
            wheelState.contactPosition = {};
            wheelState.contactNormal = {};
            wheelState.contactLongitudinal = {};
            wheelState.contactLateral = {};
            if (wheelState.hasContact)
            {
                const JPH::RVec3 contactPosition = wheel->GetContactPosition();
                const JPH::Vec3 contactNormal = wheel->GetContactNormal();
                const JPH::Vec3 contactLongitudinal = wheel->GetContactLongitudinal();
                const JPH::Vec3 contactLateral = wheel->GetContactLateral();
                const float contactLongitudinalSpeed =
                    wheel->GetContactPointVelocity().Dot(contactLongitudinal);
                wheelState.longitudinalSlipMetersPerSecond =
                    wheel->GetAngularVelocity() * wheelSettings->mRadius -
                    contactLongitudinalSpeed;
                wheelState.contactPosition = {
                    static_cast<float>(contactPosition.GetX()),
                    static_cast<float>(contactPosition.GetY()),
                    static_cast<float>(contactPosition.GetZ())};
                wheelState.contactNormal = {
                    static_cast<float>(contactNormal.GetX()),
                    static_cast<float>(contactNormal.GetY()),
                    static_cast<float>(contactNormal.GetZ())};
                wheelState.contactLongitudinal = {
                    static_cast<float>(contactLongitudinal.GetX()),
                    static_cast<float>(contactLongitudinal.GetY()),
                    static_cast<float>(contactLongitudinal.GetZ())};
                wheelState.contactLateral = {
                    static_cast<float>(contactLateral.GetX()),
                    static_cast<float>(contactLateral.GetY()),
                    static_cast<float>(contactLateral.GetZ())};
            }
        }
        m_state.sleeping = !bodyInterface.IsActive(m_impl->bodyId);
        m_state.motionObservation = BuildTankMotionObservation(m_state);
        const bool mobilityDriveRequested =
            std::abs(m_input.throttle) > 0.001f ||
            std::abs(m_input.leftTrack - 1.0f) > 0.001f ||
            std::abs(m_input.rightTrack - 1.0f) > 0.001f;
        m_state.mobility = m_mobilityStateMachine.Update(
            m_state.motionObservation,
            mobilityDriveRequested,
            deltaTimeSeconds);
    }
}
