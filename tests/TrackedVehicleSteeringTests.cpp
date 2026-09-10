#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    struct SteeringResult
    {
        float yaw = 0.0f;
        float x = 0.0f;
        float z = 0.0f;
    };

    float HeadingFrom(const Tank::Physics::Quat& rotation)
    {
        return std::atan2(
            2.0f * (rotation.w * rotation.y + rotation.x * rotation.z),
            1.0f - 2.0f * (rotation.x * rotation.x + rotation.y * rotation.y));
    }

    SteeringResult RunSteering(
        float leftTrack,
        float rightTrack,
        bool startUpsideDown = false)
    {
        constexpr float dt = 1.0f / 60.0f;
        Tank::Physics::TrackedVehicleTest test;
        Tank::Physics::TankSettings settings;
        settings.startUpsideDown = startUpsideDown;
        test.Initialize(settings);

        for (int i = 0; i < 180; ++i)
        {
            test.Step(dt);
        }

        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        input.leftTrack = leftTrack;
        input.rightTrack = rightTrack;
        test.SetInput(input);

        for (int i = 0; i < 180; ++i)
        {
            test.Step(dt);
        }

        const Tank::Physics::TrackedVehicleTestState& state = test.State();
        return {
            HeadingFrom(state.bodyRotation),
            state.bodyPosition.x,
            state.bodyPosition.z};
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle steering: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    const SteeringResult left = RunSteering(0.6f, 1.0f);
    const SteeringResult right = RunSteering(1.0f, 0.6f);
    const SteeringResult invertedLeft = RunSteering(0.6f, 1.0f, true);
    const SteeringResult invertedRight = RunSteering(1.0f, 0.6f, true);

    bool passed = true;
    passed &= Check(std::isfinite(left.yaw) && std::isfinite(right.yaw),
        "yaw values must be finite");
    passed &= Check(std::abs(left.yaw) > 0.2f && std::abs(right.yaw) > 0.2f,
        "both steering inputs must produce visible rotation");
    passed &= Check(left.yaw * right.yaw < 0.0f,
        "left and right steering must rotate in opposite directions");
    passed &= Check(std::abs(invertedLeft.yaw) > 0.2f &&
            std::abs(invertedRight.yaw) > 0.2f,
        "inverted steering inputs must produce visible rotation");
    passed &= Check(left.yaw * invertedLeft.yaw > 0.0f &&
            right.yaw * invertedRight.yaw > 0.0f,
        "upright and inverted steering must use the same control directions");
    passed &= Check(std::isfinite(left.x) && std::isfinite(left.z) &&
        std::isfinite(right.x) && std::isfinite(right.z),
        "final positions must be finite");

    if (!passed)
    {
        std::cerr << "  left yaw=" << left.yaw << " position=(" << left.x << ", " << left.z << ")\n";
        std::cerr << "  right yaw=" << right.yaw << " position=(" << right.x << ", " << right.z << ")\n";
        std::cerr << "  inverted left yaw=" << invertedLeft.yaw
            << " inverted right yaw=" << invertedRight.yaw << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle steering left_yaw=" << left.yaw
        << " right_yaw=" << right.yaw
        << " inverted_left_yaw=" << invertedLeft.yaw
        << " inverted_right_yaw=" << invertedRight.yaw << "\n";
    return 0;
}
