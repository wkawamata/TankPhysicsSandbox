#include "App/TankSettingsStore.h"

#include <cmath>
#include <filesystem>
#include <fstream>
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
            std::cerr << "FAIL TankSettingsStore: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    bool passed = true;

    // Round-trip write/read.
    {
        Tank::App::TankSettingsStore store(0);
        Tank::Physics::TankSettings source;
        source.chassisMassKg = 5200.0f;
        source.trackWidthM = 0.42f;

        std::string status;
        passed &= Check(store.Write(source, status),
            "write must succeed");

        Tank::Physics::TankSettings loaded;
        passed &= Check(store.Read(loaded, status),
            "read must succeed after write");
        passed &= Check(NearlyEqual(loaded.chassisMassKg, 5200.0f),
            "chassisMassKg must round trip");
        passed &= Check(NearlyEqual(loaded.trackWidthM, 0.42f),
            "trackWidthM must round trip");

        std::filesystem::remove(store.Path());
    }

    // Missing file.
    {
        Tank::App::TankSettingsStore store(1);
        std::filesystem::remove(store.Path());

        Tank::Physics::TankSettings settings;
        std::string status;
        passed &= Check(!store.Read(settings, status),
            "read must fail for missing file");
        passed &= Check(!status.empty(),
            "status must be set on missing file");
    }

    // Invalid JSON.
    {
        Tank::App::TankSettingsStore store(1);
        {
            std::ofstream out(store.Path(), std::ios::binary | std::ios::trunc);
            out << "{invalid}";
        }
        Tank::Physics::TankSettings settings;
        std::string status;
        passed &= Check(!store.Read(settings, status),
            "read must fail for invalid JSON");
        passed &= Check(!status.empty(),
            "status must be set on invalid JSON");

        std::filesystem::remove(store.Path());
    }

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankSettingsStore\n";
    return 0;
}
