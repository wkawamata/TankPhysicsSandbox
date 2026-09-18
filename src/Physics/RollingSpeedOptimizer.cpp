#include "RollingSpeedOptimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Tank::Physics
{
    namespace
    {
        constexpr float kDt = 1.0f / 60.0f;
        bool Cancelled(const std::atomic<bool>* cancel) { return cancel && cancel->load(); }
        Vec3 Rotate(const Quat& q, const Vec3& v)
        {
            const Vec3 t = { 2 * (q.y * v.z - q.z * v.y),
                2 * (q.z * v.x - q.x * v.z), 2 * (q.x * v.y - q.y * v.x) };
            return { v.x + q.w * t.x + q.y * t.z - q.z * t.y,
                v.y + q.w * t.y + q.z * t.x - q.x * t.z,
                v.z + q.w * t.z + q.x * t.y - q.y * t.x };
        }

        Quat Multiply(const Quat& a, const Quat& b)
        {
            return {
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
        }
    }

    RollingTrajectory ObserveRollingTrajectory(TankSettings settings,
        const PhysicsEnvironmentSettings& environment, float direction, const std::atomic<bool>* cancel)
    {
        RollingTrajectory trace;
        if (Cancelled(cancel)) { trace.error = "Cancelled"; return trace; }
        if (!settings.rollingInputEnabled) { trace.error = "Enable Rolling Input first."; return trace; }
        TrackedVehicleTest test;
        // A repeatable flat-floor calibration, with the current vehicle and floor friction.
        test.Initialize(settings, environment);
        settings = test.Settings();
        for (int step = 0; step < 600; ++step)
        {
            if (Cancelled(cancel)) { trace.error = "Cancelled"; return trace; }
            test.Step(kDt);
            if (step >= 179 && test.State().mobility.state == MobilityState::Stopped) break;
        }
        if (test.State().mobility.state != MobilityState::Stopped)
        {
            trace.error = "Vehicle did not settle before the calibration roll.";
            return trace;
        }
        const auto start = test.State();
        const Quat inverseStart = { -start.bodyRotation.x, -start.bodyRotation.y,
            -start.bodyRotation.z, start.bodyRotation.w };
        TankInput input;
        input.leftLeverX = input.rightLeverX = direction;
        test.SetInput(input);
        bool started = false;
        int finishedFrame = -1;
        for (int step = 0; step <= 480; ++step)
        {
            if (Cancelled(cancel)) { trace.error = "Cancelled"; return trace; }
            const auto& state = test.State();
            RollingTrajectorySample sample;
            sample.phase = state.rollingPhase;
            sample.movement = Rotate(inverseStart, {
                state.bodyPosition.x - start.bodyPosition.x,
                state.bodyPosition.y - start.bodyPosition.y,
                state.bodyPosition.z - start.bodyPosition.z });
            sample.rotation = Multiply(inverseStart, state.bodyRotation);
            for (int corner = 0; corner < 8; ++corner)
            {
                const Vec3 local = { (corner & 1 ? 0.5f : -0.5f) * settings.chassisWidthM,
                    corner & 2 ? 0.5f : -0.5f,
                    (corner & 4 ? 0.5f : -0.5f) * settings.chassisLengthM };
                Vec3 point = Rotate(state.bodyRotation, local);
                point.x += state.bodyPosition.x - start.bodyPosition.x;
                point.y += state.bodyPosition.y - start.bodyPosition.y;
                point.z += state.bodyPosition.z - start.bodyPosition.z;
                sample.corners[corner] = Rotate(inverseStart, point);
                const auto& p = sample.corners[corner];
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                { trace.error = "Non-finite trajectory."; return trace; }
            }
            trace.samples.push_back(sample);
            started |= state.rollingPhase != RollingPhase::None;
            if (trace.landingSeconds == 0 && state.rollingPhase == RollingPhase::Settling)
                trace.landingSeconds = step * kDt;
            if (started && finishedFrame < 0 && state.rollingPhase == RollingPhase::None)
            {
                finishedFrame = step;
                trace.finishedSeconds = step * kDt;
            }
            // Include a short stationary tail, so a fit cannot hide landing slide.
            if (finishedFrame >= 0 && step >= finishedFrame + 30) break;
            test.Step(kDt);
            input.leftLeverX = input.rightLeverX = 0;
            test.SetInput(input);
        }
        if (!trace.Complete() || trace.landingSeconds <= 0)
            trace.error = "Roll did not finish within 8 seconds.";
        return trace;
    }

    RollingTrajectoryError CompareRollingTrajectories(
        const RollingTrajectory& reference, const RollingTrajectory& candidate,
        float candidateSpeed)
    {
        RollingTrajectoryError error;
        if (reference.samples.empty() || candidate.samples.empty())
        { error.score = std::numeric_limits<float>::infinity(); return error; }
        // Continue comparing the resting endpoint if either trace has ended.
        candidateSpeed = (std::max)(candidateSpeed, 0.01f);
        const size_t referenceCount = static_cast<size_t>(std::ceil(
            reference.samples.size() / candidateSpeed));
        const size_t count = referenceCount;
        double total = 0;
        std::array<double, 8> totals = {};
        for (size_t step = 0; step < count; ++step)
        {
            const size_t referenceIndex = static_cast<size_t>(std::round(
                step * candidateSpeed));
            const auto& a = reference.samples[(std::min)(
                referenceIndex, reference.samples.size() - 1)];
            const auto& b = candidate.samples[(std::min)(step, candidate.samples.size() - 1)];
            double sampleTotal = 0;
            for (size_t corner = 0; corner < 8; ++corner)
            {
                const auto& p = a.corners[corner]; const auto& q = b.corners[corner];
                const float square = (p.x-q.x)*(p.x-q.x) + (p.y-q.y)*(p.y-q.y) + (p.z-q.z)*(p.z-q.z);
                totals[corner] += square;
                sampleTotal += square;
                error.maximumMeters = (std::max)(error.maximumMeters, std::sqrt(square));
            }
            total += sampleTotal;
            error.sampleRmsMeters.push_back(static_cast<float>(std::sqrt(sampleTotal / 8)));
        }
        error.rmsMeters = static_cast<float>(std::sqrt(total / (8 * referenceCount)));
        for (size_t i = 0; i < 8; ++i)
            error.cornerRmsMeters[i] = static_cast<float>(std::sqrt(totals[i] / referenceCount));
        error.landingTimeErrorSeconds = std::abs(candidate.landingSeconds -
            reference.landingSeconds / candidateSpeed);
        error.finishTimeErrorSeconds = std::abs(candidate.finishedSeconds -
            reference.finishedSeconds / candidateSpeed);
        error.score = error.rmsMeters * error.rmsMeters +
            0.05f * error.maximumMeters * error.maximumMeters +
            error.landingTimeErrorSeconds * error.landingTimeErrorSeconds +
            0.25f * error.finishTimeErrorSeconds * error.finishTimeErrorSeconds;
        if (!candidate.Complete()) error.score += 1000.0f;
        return error;
    }

    RollingSpeedOptimizationResult OptimizeRollingSpeed(TankSettings settings,
        const PhysicsEnvironmentSettings& environment, const std::atomic<bool>* cancel,
        const std::function<void(int, float)>& progress, int passes)
    {
        RollingSpeedOptimizationResult result;
        result.tuning = SanitizeRollingSpeedTuning(settings.rollSpeedTuning);
        settings.rollSpeedMultiplier = 1.0f;
        result.reference = ObserveRollingTrajectory(settings, environment, 1, cancel);
        if (!result.reference.Complete())
        {
            result.cancelled = Cancelled(cancel);
            result.error = "x1: " + result.reference.error;
            return result;
        }
        settings.rollSpeedMultiplier = 0.5f;
        result.slowBeforeTrajectory = ObserveRollingTrajectory(settings, environment, 1, cancel);
        result.slow = result.slowBeforeTrajectory;
        result.slowBefore = result.slowAfter = CompareRollingTrajectories(
            result.reference, result.slowBeforeTrajectory, 0.5f);
        settings.rollSpeedMultiplier = 1.5f;
        result.intermediateBeforeTrajectory = ObserveRollingTrajectory(
            settings, environment, 1, cancel);
        if (!result.intermediateBeforeTrajectory.Complete())
        {
            result.cancelled = Cancelled(cancel);
            result.error = "x1.5 before optimization: " +
                result.intermediateBeforeTrajectory.error;
            return result;
        }
        settings.rollSpeedMultiplier = 2.0f;
        result.fastBeforeTrajectory = ObserveRollingTrajectory(settings, environment, 1, cancel);
        result.optimized = result.fastBeforeTrajectory;
        result.before = result.after = CompareRollingTrajectories(
            result.reference, result.fastBeforeTrajectory);
        ++result.evaluations;
        const auto combinedScore = [](const RollingTrajectoryError& slow,
            const RollingTrajectoryError& fast)
        {
            return slow.score + fast.score + (std::max)(slow.score, fast.score);
        };
        const float originalScore = combinedScore(result.slowBefore, result.before);
        float bestScore = combinedScore(result.slowAfter, result.after);
        auto evaluate = [&](RollingSpeedTuning candidate)
        {
            if (Cancelled(cancel)) return;
            candidate = SanitizeRollingSpeedTuning(candidate);
            settings.rollSpeedTuning = candidate;
            settings.rollSpeedMultiplier = 0.5f;
            auto slowTrace = ObserveRollingTrajectory(settings, environment, 1, cancel);
            if (Cancelled(cancel)) return;
            auto slowError = CompareRollingTrajectories(
                result.reference, slowTrace, 0.5f);
            settings.rollSpeedMultiplier = 2.0f;
            auto trace = ObserveRollingTrajectory(settings, environment, 1, cancel);
            if (Cancelled(cancel)) return;
            auto error = CompareRollingTrajectories(result.reference, trace);
            ++result.evaluations;
            const float score = combinedScore(slowError, error);
            if (score < bestScore)
            {
                bestScore = score;
                result.tuning = candidate;
                result.slowAfter = std::move(slowError);
                result.slow = std::move(slowTrace);
                result.after = std::move(error);
                result.optimized = std::move(trace);
            }
            if (progress) progress(result.evaluations, std::sqrt(
                0.5f * (result.slowAfter.rmsMeters * result.slowAfter.rmsMeters +
                    result.after.rmsMeters * result.after.rmsMeters)));
        };
        // Dimensional time-scaling is only a seed. Every accepted coefficient
        // is measured with Jolt contacts, gravity and suspension enabled.
        RollingSpeedTuning physical;
        physical.gravity = 4;
        physical.suspensionFrequency = 2;
        physical.forceLimit = 4;
        physical.commitDuration = 0.5f;
        evaluate(physical);
        for (int pass = 0; pass < std::clamp(passes, 1, 12) && !Cancelled(cancel); ++pass)
        {
            const float factor = std::exp(0.35f * std::pow(0.65f, static_cast<float>(pass)));
            for (const auto& coefficient : kRollingSpeedCoefficients)
            {
                const auto center = result.tuning;
                for (float direction : { 1.0f / factor, factor })
                {
                    auto candidate = center;
                    candidate.*(coefficient.member) *= direction;
                    evaluate(candidate);
                }
            }
        }
        result.cancelled = Cancelled(cancel);
        result.improved = !result.cancelled && result.slow.Complete() &&
            result.optimized.Complete() && bestScore < originalScore;
        if (!result.cancelled && result.improved)
        {
            settings.rollSpeedTuning = result.tuning;
            settings.rollSpeedMultiplier = 1.5f;
            result.intermediate = ObserveRollingTrajectory(
                settings, environment, 1, cancel);
            result.intermediateError = CompareRollingTrajectories(
                result.reference, result.intermediate, 1.5f);
        }
        if (!result.optimized.Complete() && !result.cancelled)
            result.error = "No completed x2 roll found. Adjust the x1 profile and retry.";
        else if (!result.slow.Complete() && !result.cancelled)
            result.error = "No completed x0.5 roll found. Adjust the x1 profile and retry.";
        else if (result.improved && !result.intermediate.Complete() && !result.cancelled)
            result.error = "x1.5 measurement did not finish.";
        return result;
    }
}
