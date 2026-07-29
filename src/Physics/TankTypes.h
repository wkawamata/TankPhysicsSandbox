#pragma once

#include "PhysicsTypes.h"

#include <array>

namespace Tank::Physics
{
    constexpr int kTankTrackCount = 2;
    constexpr int kTankMaxRoadWheelCount = 4;
    constexpr int kTankMaxWheelsPerSurface = kTankMaxRoadWheelCount + 2;
    constexpr int kTankSurfacesPerTrack = 2;
    constexpr int kTankMaxWheelsPerTrack =
        kTankMaxWheelsPerSurface * kTankSurfacesPerTrack;
    constexpr int kTankWheelCount = kTankTrackCount * kTankMaxWheelsPerTrack;

    struct TankSettings
    {
        float chassisMassKg = 4000.0f;
        bool rollingInputEnabled = true;
        float rollTorqueNm = 120000.0f;
        float rollDistanceM = 2.4f;
        float rollTorqueCutoffDegrees = 90.0f;
        float rollStabilizationTorqueNm = 30000.0f;
        float rollStabilizationDampingNms = 10000.0f;
        float trackWidthM = 0.3f;
        float trackSpacingM = 2.4f;
        float chassisWidthM = 2.4f;
        float chassisLengthM = 4.0f;
        float wheelRadiusM = 0.3f;
        int roadWheelCount = 3;
        float rideHeightScale = 0.8f;
        bool startUpsideDown = false;
    };

    struct TankInput
    {
        float throttle = 0.0f;
        float steering = 0.0f;
        float leftTrack = 1.0f;
        float rightTrack = 1.0f;
        float roll = 0.0f;
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
        bool upperSurface = false;
        TransformState transform = {};
        float suspensionLength = 0.0f;
        bool hasContact = false;
    };

    struct TankState
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        TransformState body = {};
        Vec3 linearVelocity = {};
        Vec3 angularVelocity = {};
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
        bool sleeping = false;
    };
}
