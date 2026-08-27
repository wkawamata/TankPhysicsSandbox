#include "TankMotionObservation.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    namespace
    {
        bool IsFinite(float value)
        {
            return std::isfinite(value);
        }

        bool IsFinite(const Vec3& value)
        {
            return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
        }

        bool IsFinite(const Quat& value)
        {
            return IsFinite(value.x) && IsFinite(value.y) &&
                IsFinite(value.z) && IsFinite(value.w);
        }

        float Dot(const Vec3& lhs, const Vec3& rhs)
        {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        float Length(const Vec3& value)
        {
            return std::sqrt(Dot(value, value));
        }

        Vec3 RotateVector(const Quat& rotation, const Vec3& value)
        {
            const float tx = 2.0f * (rotation.y * value.z - rotation.z * value.y);
            const float ty = 2.0f * (rotation.z * value.x - rotation.x * value.z);
            const float tz = 2.0f * (rotation.x * value.y - rotation.y * value.x);
            return {
                value.x + rotation.w * tx + rotation.y * tz - rotation.z * ty,
                value.y + rotation.w * ty + rotation.z * tx - rotation.x * tz,
                value.z + rotation.w * tz + rotation.x * ty - rotation.y * tx};
        }

        Vec3 Add(const Vec3& lhs, const Vec3& rhs)
        {
            return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
        }

        Vec3 Divide(const Vec3& value, float divisor)
        {
            return {value.x / divisor, value.y / divisor, value.z / divisor};
        }
    }

    TankMotionObservation BuildTankMotionObservation(const TankState& state)
    {
        TankMotionObservation observation;
        observation.stepIndex = state.stepIndex;
        observation.timeSeconds = state.timeSeconds;
        observation.bodyRight = RotateVector(state.body.rotation, {1.0f, 0.0f, 0.0f});
        observation.bodyUp = RotateVector(state.body.rotation, {0.0f, 1.0f, 0.0f});
        observation.bodyForward = RotateVector(state.body.rotation, {0.0f, 0.0f, 1.0f});
        observation.localLinearVelocity = {
            Dot(state.linearVelocity, observation.bodyRight),
            Dot(state.linearVelocity, observation.bodyUp),
            Dot(state.linearVelocity, observation.bodyForward)};
        observation.localAngularVelocity = {
            Dot(state.angularVelocity, observation.bodyRight),
            Dot(state.angularVelocity, observation.bodyUp),
            Dot(state.angularVelocity, observation.bodyForward)};
        observation.linearSpeedMetersPerSecond = Length(state.linearVelocity);
        observation.horizontalSpeedMetersPerSecond = std::sqrt(
            state.linearVelocity.x * state.linearVelocity.x +
            state.linearVelocity.z * state.linearVelocity.z);
        observation.angularSpeedRadiansPerSecond = Length(state.angularVelocity);
        observation.allFinite =
            IsFinite(state.timeSeconds) && IsFinite(state.body.position) &&
            IsFinite(state.body.rotation) && IsFinite(state.linearVelocity) &&
            IsFinite(state.angularVelocity) && IsFinite(observation.bodyRight) &&
            IsFinite(observation.bodyUp) && IsFinite(observation.bodyForward) &&
            IsFinite(observation.localLinearVelocity) &&
            IsFinite(observation.localAngularVelocity) &&
            IsFinite(observation.linearSpeedMetersPerSecond) &&
            IsFinite(observation.horizontalSpeedMetersPerSecond) &&
            IsFinite(observation.angularSpeedRadiansPerSecond);

        std::array<Vec3, kTankTrackCount> trackNormalSums = {};
        Vec3 totalNormalSum = {};
        const int wheelCount = std::clamp(state.wheelCount, 0, kTankWheelCount);
        for (int index = 0; index < wheelCount; ++index)
        {
            const TrackedWheelState& wheel = state.wheels[static_cast<size_t>(index)];
            observation.allFinite = observation.allFinite &&
                IsFinite(wheel.suspensionVelocityMetersPerSecond) &&
                IsFinite(wheel.angularVelocityRadians) &&
                IsFinite(wheel.suspensionImpulseNewtonSeconds) &&
                IsFinite(wheel.longitudinalImpulseNewtonSeconds) &&
                IsFinite(wheel.lateralImpulseNewtonSeconds) &&
                IsFinite(wheel.longitudinalSlipMetersPerSecond);
            if (wheel.trackIndex < 0 || wheel.trackIndex >= kTankTrackCount)
            {
                observation.allFinite = false;
                continue;
            }
            if (!wheel.hasContact)
            {
                continue;
            }

            observation.allFinite = observation.allFinite &&
                IsFinite(wheel.contactPosition) && IsFinite(wheel.contactNormal) &&
                IsFinite(wheel.contactLongitudinal) && IsFinite(wheel.contactLateral);
            TrackContactObservation& track =
                observation.tracks[static_cast<size_t>(wheel.trackIndex)];
            ++track.contactCount;
            ++observation.totalContactCount;
            if (wheel.upperSurface)
            {
                ++track.upperSurfaceContactCount;
                ++observation.totalUpperSurfaceContactCount;
            }
            else
            {
                ++track.lowerSurfaceContactCount;
                ++observation.totalLowerSurfaceContactCount;
            }
            trackNormalSums[static_cast<size_t>(wheel.trackIndex)] = Add(
                trackNormalSums[static_cast<size_t>(wheel.trackIndex)],
                wheel.contactNormal);
            totalNormalSum = Add(totalNormalSum, wheel.contactNormal);
            const float absoluteSlip = std::abs(wheel.longitudinalSlipMetersPerSecond);
            track.maximumAbsoluteLongitudinalSlipMetersPerSecond = (std::max)(
                track.maximumAbsoluteLongitudinalSlipMetersPerSecond,
                absoluteSlip);
            track.averageAbsoluteLongitudinalSlipMetersPerSecond += absoluteSlip;
            track.maximumAbsoluteSuspensionVelocityMetersPerSecond = (std::max)(
                track.maximumAbsoluteSuspensionVelocityMetersPerSecond,
                std::abs(wheel.suspensionVelocityMetersPerSecond));
            track.hasSuspensionHardPoint =
                track.hasSuspensionHardPoint || wheel.suspensionAtHardPoint;
        }

        for (int trackIndex = 0; trackIndex < kTankTrackCount; ++trackIndex)
        {
            TrackContactObservation& track =
                observation.tracks[static_cast<size_t>(trackIndex)];
            if (track.contactCount > 0)
            {
                const float count = static_cast<float>(track.contactCount);
                track.averageContactNormal = Divide(
                    trackNormalSums[static_cast<size_t>(trackIndex)], count);
                track.averageAbsoluteLongitudinalSlipMetersPerSecond /= count;
            }
            observation.allFinite = observation.allFinite &&
                IsFinite(track.averageContactNormal) &&
                IsFinite(track.maximumAbsoluteLongitudinalSlipMetersPerSecond) &&
                IsFinite(track.averageAbsoluteLongitudinalSlipMetersPerSecond) &&
                IsFinite(track.maximumAbsoluteSuspensionVelocityMetersPerSecond);
        }

        if (observation.totalContactCount > 0)
        {
            observation.averageContactNormal = Divide(
                totalNormalSum,
                static_cast<float>(observation.totalContactCount));
        }
        observation.hasLeftDriveContact =
            observation.tracks[0].lowerSurfaceContactCount > 0;
        observation.hasRightDriveContact =
            observation.tracks[1].lowerSurfaceContactCount > 0;
        observation.hasRequiredDriveContact =
            observation.hasLeftDriveContact && observation.hasRightDriveContact;
        observation.allFinite = observation.allFinite &&
            IsFinite(observation.averageContactNormal);
        return observation;
    }
}
