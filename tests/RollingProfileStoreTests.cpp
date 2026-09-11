#include "App/RollingProfileStore.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL RollingProfileStore: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    bool passed = true;
    const std::filesystem::path testRoot =
        std::filesystem::current_path() / "RollingProfileStoreTestsTemp";
    std::filesystem::remove_all(testRoot);

    Tank::Physics::TankSettings sourceSettings;
    sourceSettings.chassisWidthM = 8.0f;
    sourceSettings.rollTorqueNm = 123456.0f;
    sourceSettings.rollTravelVehicleWidths = 1.75f;
    const Tank::Physics::RollingProfile source =
        Tank::Physics::ExtractRollingProfile(sourceSettings);

    Tank::App::RollingProfileStore store(1, testRoot);
    std::string status;
    passed &= Check(store.Write(source, status), "write must succeed");

    Tank::Physics::RollingProfile loaded;
    passed &= Check(store.Read(loaded, status), "read must succeed after write");
    passed &= Check(NearlyEqual(loaded.torqueNm, 123456.0f),
        "rolling torque must round trip");
    passed &= Check(NearlyEqual(loaded.travelVehicleWidths, 1.75f),
        "travel widths must round trip");

    Tank::Physics::TankSettings targetSettings;
    targetSettings.chassisWidthM = 2.6f;
    targetSettings.trackWidthM = 0.45f;
    Tank::Physics::ApplyRollingProfile(loaded, targetSettings);
    passed &= Check(NearlyEqual(targetSettings.rollTorqueNm, 123456.0f),
        "apply must update rolling torque");
    passed &= Check(NearlyEqual(targetSettings.chassisWidthM, 2.6f),
        "apply must not update chassis width");
    passed &= Check(NearlyEqual(targetSettings.trackWidthM, 0.45f),
        "apply must not update track width");

    std::filesystem::remove_all(testRoot);
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS RollingProfileStore\n";
    return 0;
}
