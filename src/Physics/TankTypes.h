#pragma once

#include "PhysicsTypes.h"

#include <array>

namespace Tank::Physics
{
    constexpr int kTankTrackCount = 2;
    constexpr int kTankWheelsPerTrack = 5;
    constexpr int kTankWheelCount = kTankTrackCount * kTankWheelsPerTrack;

    struct TankInput
    {
        float throttle = 0.0f;
        float steering = 0.0f;
        float leftTrack = 0.0f;
        float rightTrack = 0.0f;
        bool brake = false;
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
