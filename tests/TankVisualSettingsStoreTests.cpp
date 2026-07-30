#include "App/TankVisualSettingsStore.h"

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
            std::cerr << "FAIL TankVisualSettingsStore: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    bool passed = true;

    // Round-trip write/read.
    {
        Tank::App::TankVisualSettingsStore store;
        Tank::Rendering::TankVisualSettings source;
        source.hullUpper.albedo = { 0.1f, 0.2f, 0.3f };
        source.wheels.metallic = 0.9f;

        std::string status;
        passed &= Check(store.Write(source, status),
            "write must succeed");

        Tank::Rendering::TankVisualSettings loaded;
        passed &= Check(store.Read(loaded, status),
            "read must succeed after write");
        passed &= Check(NearlyEqual(loaded.hullUpper.albedo.r, 0.1f),
            "albedo must round trip");
        passed &= Check(NearlyEqual(loaded.wheels.metallic, 0.9f),
            "wheel metallic must round trip");

        std::filesystem::remove(store.Path());
    }

    // Missing file.
    {
        Tank::App::TankVisualSettingsStore store;
        std::filesystem::remove(store.Path());

        Tank::Rendering::TankVisualSettings settings;
        std::string status;
        passed &= Check(!store.Read(settings, status),
            "read must fail for missing file");
        passed &= Check(!status.empty(),
            "status must be set on missing file");
    }

    // Invalid JSON.
    {
        Tank::App::TankVisualSettingsStore store;
        {
            std::ofstream out(store.Path(), std::ios::binary | std::ios::trunc);
            out << "{invalid}";
        }
        Tank::Rendering::TankVisualSettings settings;
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

    std::cout << "PASS TankVisualSettingsStore\n";
    return 0;
}
