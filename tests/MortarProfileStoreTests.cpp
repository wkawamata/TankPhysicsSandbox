#include "App/MortarProfileStore.h"
#include <cmath>
#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path root = std::filesystem::current_path() / "MortarProfileStoreTestsTemp";
    std::filesystem::remove_all(root);
    Tank::Physics::TankSettings sourceSettings;
    sourceSettings.mortarMaximumAngleDegrees = 44.0f;
    sourceSettings.mortarMaximumRangeMeters = 72.0f;
    sourceSettings.mortarStanceTorqueNm = 640000.0f;
    const auto source = Tank::Physics::ExtractMortarProfile(sourceSettings);
    Tank::App::MortarProfileStore store(2, root);
    std::string status;
    Tank::Physics::MortarProfile loaded;
    bool passed = store.Write(source, status) && store.Read(loaded, status);
    Tank::Physics::TankSettings target;
    const float originalChassisMass = target.chassisMassKg;
    Tank::Physics::ApplyMortarProfile(loaded, target);
    passed &= std::abs(target.mortarMaximumAngleDegrees - 44.0f) < 0.001f;
    passed &= std::abs(target.mortarMaximumRangeMeters - 72.0f) < 0.001f;
    passed &= std::abs(target.mortarStanceTorqueNm - 640000.0f) < 0.001f;
    passed &= target.chassisMassKg == originalChassisMass;
    std::filesystem::remove_all(root);
    if (!passed) { std::cerr << "FAIL MortarProfileStore\n"; return 1; }
    std::cout << "PASS MortarProfileStore\n";
    return 0;
}
