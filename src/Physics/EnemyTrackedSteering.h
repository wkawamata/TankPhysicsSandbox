#pragma once
#include "TankTypes.h"
#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    // Route intent becomes track inputs. No direct forces or pose correction.
    inline TankInput EnemyTrackedSteering(const TankState& state, const Vec3& target,
        float speedLimit, float brakingDeceleration, float arrivalMeters, float preferredTurnSign = 1)
    {
        TankInput input;
        input.brake = true;
        const float dx = target.x-state.body.position.x, dz = target.z-state.body.position.z;
        const float distance = std::hypot(dx, dz);
        const float speed = std::hypot(state.linearVelocity.x, state.linearVelocity.z);
        if (distance <= arrivalMeters) return input;
        const auto& q = state.body.rotation;
        const float yaw = std::atan2(2*(q.w*q.y+q.x*q.z), 1-2*(q.y*q.y+q.z*q.z));
        float error = std::remainder(std::atan2(dx, dz)-yaw, 6.283185307f);
        if (std::abs(error) > 2.8f) error = std::copysign(std::abs(error), preferredTurnSign);
        if (std::abs(error) > 0.3f)
        {
            // Stop translation before a pivot. Slow the pivot as alignment approaches.
            if (speed > 0.2f) return input;
            const float desiredYawSpeed = std::min(0.6f, std::abs(error));
            if (state.angularVelocity.y * std::copysign(1.0f, error) > desiredYawSpeed) return input;
            input.brake = false;
            input.throttle = 1.0f;
            // TankController track 0 is on +X; slow/reverse it for positive yaw.
            input.leftTrack = -std::copysign(1.0f, error);
            input.rightTrack = -input.leftTrack;
            return input;
        }
        const float desiredSpeed = std::min(speedLimit,
            std::sqrt(2*brakingDeceleration*std::max(0.0f, distance-arrivalMeters*0.5f)));
        if (speed > desiredSpeed + 0.1f) return input;
        input.brake = false;
        input.throttle = std::clamp((desiredSpeed-speed)*0.4f, 0.0f, 0.35f);
        const float turn = std::clamp(error*2, -0.6f, 0.6f);
        input.leftTrack = turn > 0 ? 1-turn : 1;
        input.rightTrack = turn < 0 ? 1+turn : 1;
        return input;
    }
}