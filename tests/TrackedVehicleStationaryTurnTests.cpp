#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    struct TurnResult
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

    TurnResult RunTurn(float leftTrack, float rightTrack)
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
            std::cerr << "FAIL TrackedVehicle stationary turn: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    const TurnResult left = RunTurn(0.0f, 1.0f);
    const TurnResult right = RunTurn(1.0f, 0.0f);

    bool passed = true;
    passed &= Check(std::isfinite(left.yaw) && std::isfinite(right.yaw),
        "yaw values must be finite");
    passed &= Check(std::abs(left.yaw) > 0.2f && std::abs(right.yaw) > 0.2f,
        "both stationary-turn inputs must produce visible rotation");
    passed &= Check(left.yaw * right.yaw < 0.0f,
        "left and right stationary turns must rotate in opposite directions");
    passed &= Check(left.horizontalDistance < 4.0f && right.horizontalDistance < 4.0f,
        "stationary turns must remain near their starting points");

    if (!passed)
    {
        std::cerr << "  left yaw=" << left.yaw << " distance=" << left.horizontalDistance << "\n";
        std::cerr << "  right yaw=" << right.yaw << " distance=" << right.horizontalDistance << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle stationary turn left_yaw=" << left.yaw
        << " right_yaw=" << right.yaw
        << " left_distance=" << left.horizontalDistance
        << " right_distance=" << right.horizontalDistance << "\n";
    return 0;
}
