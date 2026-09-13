#pragma once

#include "TrackedVehicleTest.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Tank::Physics
{
    struct RollingTrajectorySample
    {
        Vec3 movement = {};
        Quat rotation = {};
        std::array<Vec3, 8> corners;
        RollingPhase phase = RollingPhase::None;
    };

    struct RollingTrajectory
    {
        std::vector<RollingTrajectorySample> samples;
        float landingSeconds = 0.0f;
        float finishedSeconds = 0.0f;
        std::string error;
        bool Complete() const { return error.empty() && finishedSeconds > 0.0f; }
    };

    struct RollingTrajectoryError
    {
        float rmsMeters = 0.0f;
        float maximumMeters = 0.0f;
        float landingTimeErrorSeconds = 0.0f;
        float finishTimeErrorSeconds = 0.0f;
        float score = 0.0f;
        std::array<float, 8> cornerRmsMeters = {};
        std::vector<float> sampleRmsMeters;
    };

    struct RollingSpeedOptimizationResult
    {
        RollingSpeedTuning tuning;
        RollingTrajectory slow;
        RollingTrajectory reference;
        RollingTrajectory intermediate;
        RollingTrajectory optimized;
        RollingTrajectoryError slowBefore;
        RollingTrajectoryError slowAfter;
        RollingTrajectoryError before;
        RollingTrajectoryError intermediateError;
        RollingTrajectoryError after;
        int evaluations = 0;
        bool improved = false;
        bool cancelled = false;
        std::string error;
    };

    // Eight corners of the rigid chassis Local BB, transformed into the
    // starting tank frame. Rendering and changing world AABBs are not used.
    RollingTrajectory ObserveRollingTrajectory(
        TankSettings settings, const PhysicsEnvironmentSettings& environment = {},
        float direction = 1.0f, const std::atomic<bool>* cancel = nullptr);
    // Compares candidate(t) with reference(2*t), including post-landing motion.
    RollingTrajectoryError CompareRollingTrajectories(
        const RollingTrajectory& reference, const RollingTrajectory& candidate,
        float candidateSpeed = 2.0f);
    RollingSpeedOptimizationResult OptimizeRollingSpeed(
        TankSettings settings, const PhysicsEnvironmentSettings& environment = {},
        const std::atomic<bool>* cancel = nullptr,
        const std::function<void(int, float)>& progress = {}, int passes = 4);
}
