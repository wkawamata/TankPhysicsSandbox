#include "Physics/TrackedVehicleTest.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
    struct PivotResult
    {
        float yaw = 0.0f;
        float horizontalDistance = 0.0f;
    };

    float YawFrom(const Tank::Physics::Quat& rotation)
    {
        return std::atan2(
            2.0f * (rotation.w * rotation.y + rotation.x * rotation.z),
            1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z));
    }

    PivotResult RunPivot(float leftTrack, float rightTrack)
    {
        constexpr float dt = 1.0f / 60.0f;
        Tank::Physics::TrackedVehicleTest test;
        test.Initialize();

        for (int i = 0; i < 180; ++i)
        {
            test.Step(dt);
        }

        const Tank::Physics::Vec3 start = test.State().bodyPosition;

        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        input.leftTrack = leftTrack;
        input.rightTrack = rightTrack;
        test.SetInput(input);

        for (int i = 0; i < 90; ++i)
        {
            test.Step(dt);
        }

        const Tank::Physics::TrackedVehicleTestState& state = test.State();
        const float dx = state.bodyPosition.x - start.x;
        const float dz = state.bodyPosition.z - start.z;
        return {YawFrom(state.bodyRotation), std::sqrt(dx * dx + dz * dz)};
    }

    float RunLimitedPivot(float yawSpeedLimitDegrees)
    {
        constexpr float dt = 1.0f / 60.0f;
        Tank::Physics::TankSettings settings;
        settings.yawSpeedLimitDegrees = yawSpeedLimitDegrees;
        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings);

        for (int i = 0; i < 180; ++i)
        {
            test.Step(dt);
        }

        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        input.leftTrack = -1.0f;
        input.rightTrack = 1.0f;
        test.SetInput(input);

        float maximumYawSpeed = 0.0f;
        for (int i = 0; i < 120; ++i)
        {
            const Tank::Physics::TrackedVehicleTestState state = test.Step(dt);
            const Tank::Physics::Quat& q = state.bodyRotation;
            const Tank::Physics::Vec3 bodyUp = {
                2.0f * (q.x * q.y - q.w * q.z),
                1.0f - 2.0f * (q.x * q.x + q.z * q.z),
                2.0f * (q.y * q.z + q.w * q.x) };
            const float yawSpeed =
                state.angularVelocity.x * bodyUp.x +
                state.angularVelocity.y * bodyUp.y +
                state.angularVelocity.z * bodyUp.z;
            maximumYawSpeed = std::max(maximumYawSpeed, std::abs(yawSpeed));
        }
        return maximumYawSpeed;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle pivot: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    const PivotResult left = RunPivot(-1.0f, 1.0f);
    const PivotResult right = RunPivot(1.0f, -1.0f);
    constexpr float limitedYawDegrees = 30.0f;
    const float limitedYawSpeed = RunLimitedPivot(limitedYawDegrees);

    bool passed = true;
    passed &= Check(std::isfinite(left.yaw) && std::isfinite(right.yaw),
        "yaw values must be finite");
    passed &= Check(std::abs(left.yaw) > 0.3f && std::abs(right.yaw) > 0.3f,
        "both pivot inputs must produce visible rotation");
    passed &= Check(left.yaw * right.yaw < 0.0f,
        "left and right pivot inputs must rotate in opposite directions");
    passed &= Check(left.horizontalDistance < 2.0f && right.horizontalDistance < 2.0f,
        "pivot turn must keep the body near its starting point");
    passed &= Check(
        limitedYawSpeed <= limitedYawDegrees * 3.14159265358979323846f / 180.0f + 0.001f,
        "body yaw speed must respect the configured limit");

    if (!passed)
    {
        std::cerr << "  left yaw=" << left.yaw << " distance=" << left.horizontalDistance << "\n";
        std::cerr << "  right yaw=" << right.yaw << " distance=" << right.horizontalDistance << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle pivot left_yaw=" << left.yaw
        << " right_yaw=" << right.yaw
        << " left_distance=" << left.horizontalDistance
        << " right_distance=" << right.horizontalDistance << "\n";
    return 0;
}
