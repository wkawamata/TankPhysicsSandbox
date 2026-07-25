#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    float SettledBodyHeight(float rideHeightScale)
    {
        constexpr float dt = 1.0f / 60.0f;

        Tank::Physics::TankSettings settings;
        settings.rideHeightScale = rideHeightScale;

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings);
        for (int i = 0; i < 300; ++i)
        {
            test.Step(dt);
        }
        return test.State().bodyPosition.y;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle ride height: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    const float raisedHeight = SettledBodyHeight(0.9f);
    const float defaultHeight = SettledBodyHeight(0.8f);
    const float loweredHeight = SettledBodyHeight(0.7f);

    bool passed = true;
    passed &= Check(
        std::isfinite(raisedHeight) &&
            std::isfinite(defaultHeight) &&
            std::isfinite(loweredHeight),
        "settled heights must be finite");
    passed &= Check(
        loweredHeight < defaultHeight &&
            defaultHeight < raisedHeight,
        "70, 80 and 90 percent settings must produce ordered chassis heights");

    if (!passed)
    {
        std::cerr << "  raisedHeight=" << raisedHeight
            << " defaultHeight=" << defaultHeight
            << " loweredHeight=" << loweredHeight << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle ride height raised=" << raisedHeight
        << " default=" << defaultHeight
        << " lowered=" << loweredHeight << "\n";
    return 0;
}
