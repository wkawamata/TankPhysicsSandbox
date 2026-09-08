#pragma once

#include "PhysicsTypes.h"
#include "MobilityTypes.h"
#include "SpecialMoveTypes.h"
#include "MortarAimController.h"

#include <array>

namespace Tank::Physics
{
    constexpr int kTankTrackCount = 2;
    constexpr int kTankMaxRoadWheelCount = 4;
    constexpr int kTankMaxWheelsPerSurface = kTankMaxRoadWheelCount + 2;
    constexpr int kTankSurfacesPerTrack = 2;
    constexpr int kTankSuspensionPositionsPerSurface =
        kTankMaxRoadWheelCount + 2;
    constexpr int kTankMaxWheelsPerTrack =
        kTankMaxWheelsPerSurface * kTankSurfacesPerTrack;
    constexpr int kTankWheelCount = kTankTrackCount * kTankMaxWheelsPerTrack;

    constexpr int TankSuspensionSlotIndex(
        int trackIndex, int surfaceIndex, int positionIndex)
    {
        return trackIndex *
                (kTankSurfacesPerTrack * kTankSuspensionPositionsPerSurface) +
            surfaceIndex * kTankSuspensionPositionsPerSurface + positionIndex;
    }

    constexpr std::array<float, kTankWheelCount> MakeDefaultSuspensionStrokes(
        float rideHeightScale = 0.8f)
    {
        std::array<float, kTankWheelCount> strokes = {};
        for (int track = 0; track < kTankTrackCount; ++track)
        {
            for (int surface = 0; surface < kTankSurfacesPerTrack; ++surface)
            {
                for (int road = 1; road <= kTankMaxRoadWheelCount; ++road)
                {
                    strokes[static_cast<size_t>(TankSuspensionSlotIndex(
                        track, surface, road))] = 0.2f * rideHeightScale;
                }
            }
        }
        return strokes;
    }

    struct TankSettings
    {
        float chassisMassKg = 4000.0f;
        float recoilImpulseNewtonSeconds = 20000.0f;
        float recoilPointForwardM = 1.2f;
        float recoilPointHeightM = 0.8f;
        bool rollingInputEnabled = true;
        float rollTorqueNm = 200000.0f;
        float rollApproachDampingNms = 30000.0f;
        float rollCommitTorqueNm = 100000.0f;
        float rollAirBrakeTorqueNm = 90000.0f;
        float rollAirBrakeReleaseDegrees = 30.0f;
        bool rollDistanceMatchesVehicleWidth = true;
        float rollTravelVehicleWidths = 1.0f;
        float rollDistanceM = 2.4f;
        float rollTorqueCutoffDegrees = 90.0f;
        float rollStabilizationTorqueNm = 30000.0f;
        float rollStabilizationDampingNms = 10000.0f;
        float trackWidthM = 0.3f;
        float trackSpacingM = 2.4f;
        float trackLongitudinalFriction = 4.0f;
        float trackLateralFriction = 2.0f;
        float chassisWidthM = 2.4f;
        float chassisLengthM = 4.0f;
        float endWheelRadiusM = 0.4f;
        float roadWheelRadiusM = 0.3f;
        int roadWheelCount = 3;
        float endWheelOffsetM = 0.0f;
        float endWheelVerticalOffsetM = 0.0f;
        float roadWheelVerticalOffsetM = 0.0f;
        float wheelHorizontalOffsetM = 0.0f;
        float twoRoadWheelOffsetM = 0.67f;
        float threeRoadWheelOffsetM = 1.0f;
        float rideHeightScale = 0.8f;
        float suspensionFrequencyHz = 1.0f;
        float suspensionDamping = 0.5f;
        std::array<float, kTankWheelCount> suspensionStrokeMeters =
            MakeDefaultSuspensionStrokes();
        bool neutralBrakeEnabled = true;
        float neutralBrakeAmount = 0.30f;
        float stationaryTurnInnerTrackRatio = 0.0f;
        float stationaryTurnLeftTraction = 1.0f;
        float stationaryTurnRightTraction = 1.0f;
        float pivotTurnLeftTraction = 1.0f;
        float pivotTurnRightTraction = 1.0f;
        float engineMaxTorqueNm = 900.0f;
        float engineMaxRpm = 5000.0f;
        float transmissionShiftDownRpm = 1000.0f;
        float transmissionShiftUpRpm = 4375.0f;
        float transmissionClutchStrength = 10.0f;
        float finalDriveRatio = 1.0f;
        float clutchReleaseTimeSeconds = 0.03f;
        float yawSpeedLimitDegrees = 720.0f;
        float yawDamping = 0.0f;
        bool startUpsideDown = false;
        float stoppedEnterLinearSpeedMetersPerSecond = 0.20f;
        float stoppedExitLinearSpeedMetersPerSecond = 0.35f;
        float stoppedEnterAngularSpeedRadiansPerSecond = 0.20f;
        float stoppedExitAngularSpeedRadiansPerSecond = 0.35f;
        float stoppedEnterTrackSlipMetersPerSecond = 0.20f;
        float stoppedExitTrackSlipMetersPerSecond = 0.40f;
        float stoppedEnterSuspensionSpeedMetersPerSecond = 0.10f;
        float stoppedExitSuspensionSpeedMetersPerSecond = 0.20f;
        float stoppedMinimumUpAlignment = 0.90f;
        float stoppedConfirmSeconds = 0.25f;
    };

    struct TankInput
    {
        float throttle = 0.0f;
        float steering = 0.0f;
        float leftTrack = 1.0f;
        float rightTrack = 1.0f;
        float roll = 0.0f;
        float leftLeverX = 0.0f;
        float rightLeverX = 0.0f;
        float brakeAmount = 0.0f;
        bool brake = false;
    };

    struct TrackedDriverInput
    {
        float forward = 0.0f;
        float leftRatio = 1.0f;
        float rightRatio = 1.0f;
        float brake = 0.0f;
    };

    struct TransformState
    {
        Vec3 position = {};
        Quat rotation = {};
    };

    struct TrackedWheelState
    {
        int trackIndex = 0;
        int wheelIndex = 0;
        int suspensionSlotIndex = 0;
        bool upperSurface = false;
        TransformState transform = {};
        Vec3 suspensionOrigin = {};
        Vec3 suspensionDirection = {};
        float suspensionLength = 0.0f;
        float suspensionMinLength = 0.0f;
        float suspensionMaxLength = 0.0f;
        float suspensionVelocityMetersPerSecond = 0.0f;
        bool suspensionAtHardPoint = false;
        float angularVelocityRadians = 0.0f;
        float suspensionImpulseNewtonSeconds = 0.0f;
        float longitudinalImpulseNewtonSeconds = 0.0f;
        float lateralImpulseNewtonSeconds = 0.0f;
        float longitudinalSlipMetersPerSecond = 0.0f;
        bool hasContact = false;
        Vec3 contactPosition = {};
        Vec3 contactNormal = {};
        Vec3 contactLongitudinal = {};
        Vec3 contactLateral = {};
    };

    struct TrackContactObservation
    {
        int contactCount = 0;
        int lowerSurfaceContactCount = 0;
        int upperSurfaceContactCount = 0;
        Vec3 averageContactNormal = {};
        float maximumAbsoluteLongitudinalSlipMetersPerSecond = 0.0f;
        float averageAbsoluteLongitudinalSlipMetersPerSecond = 0.0f;
        float maximumAbsoluteSuspensionVelocityMetersPerSecond = 0.0f;
        bool hasSuspensionHardPoint = false;
    };

    struct TankMotionObservation
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        Vec3 bodyRight = {};
        Vec3 bodyUp = {};
        Vec3 bodyForward = {};
        Vec3 localLinearVelocity = {};
        Vec3 localAngularVelocity = {};
        float linearSpeedMetersPerSecond = 0.0f;
        float horizontalSpeedMetersPerSecond = 0.0f;
        float angularSpeedRadiansPerSecond = 0.0f;
        std::array<TrackContactObservation, kTankTrackCount> tracks = {};
        int totalContactCount = 0;
        int totalLowerSurfaceContactCount = 0;
        int totalUpperSurfaceContactCount = 0;
        Vec3 averageContactNormal = {};
        bool hasLeftDriveContact = false;
        bool hasRightDriveContact = false;
        bool hasRequiredDriveContact = false;
        bool allFinite = true;
    };

    struct TankState
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        TransformState body = {};
        Vec3 linearVelocity = {};
        Vec3 angularVelocity = {};
        float speedMetersPerSecond = 0.0f;
        float maximumSpeedMetersPerSecond = 0.0f;
        float zeroToTenTimeSeconds = -1.0f;
        float engineRpm = 0.0f;
        int transmissionGear = 0;
        float clutchFriction = 0.0f;
        float yawSpeedDegrees = 0.0f;
        bool yawSpeedLimited = false;
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
        bool sleeping = false;
        TankMotionObservation motionObservation = {};
        MobilityStateSnapshot mobility = {};
        RollingPhase rollingPhase = RollingPhase::None;
        SpecialMoveStateSnapshot specialMove = {};
        MortarAimSnapshot mortarAim = {};
        bool trackInputSwapped = false;
        bool rollChainAvailable = false;
    };
}
