#include "Physics/TrackedVehicleTest.h"

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

    bool passed = true;
    passed &= Check(std::isfinite(left.yaw) && std::isfinite(right.yaw),
        "yaw values must be finite");
    passed &= Check(std::abs(left.yaw) > 0.3f && std::abs(right.yaw) > 0.3f,
        "both pivot inputs must produce visible rotation");
    passed &= Check(left.yaw * right.yaw < 0.0f,
        "left and right pivot inputs must rotate in opposite directions");
    passed &= Check(left.horizontalDistance < 2.0f && right.horizontalDistance < 2.0f,
        "pivot turn must keep the body near its starting point");

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
