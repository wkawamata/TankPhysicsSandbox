#include "App/CameraSettingsStore.h"

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
            std::cerr << "FAIL CameraSettingsStore: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    bool passed = true;

    // Round-trip write/read.
    {
        Tank::App::CameraSettingsStore store(0);
        Tank::Rendering::CameraSettings source;
        source.position[0] = 3.0f;
        source.fovDegrees = 45.0f;
        source.followTank = true;

        std::string status;
        passed &= Check(store.Write(source, status),
            "write must succeed");

        Tank::Rendering::CameraSettings loaded;
        passed &= Check(store.Read(loaded, status),
            "read must succeed after write");
        passed &= Check(NearlyEqual(loaded.position[0], 3.0f),
            "position must round trip");
        passed &= Check(loaded.followTank,
            "followTank must round trip");

        std::filesystem::remove(store.Path());
    }

    // Missing file.
    {
        Tank::App::CameraSettingsStore store(0);
        std::filesystem::remove(store.Path());

        Tank::Rendering::CameraSettings settings;
        std::string status;
        passed &= Check(!store.Read(settings, status),
            "read must fail for missing file");
        passed &= Check(!status.empty(),
            "status must be set on missing file");
    }

    // Invalid JSON.
    {
        Tank::App::CameraSettingsStore store(0);
        {
            std::ofstream out(store.Path(), std::ios::binary | std::ios::trunc);
            out << "{invalid}";
        }
        Tank::Rendering::CameraSettings settings;
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

    std::cout << "PASS CameraSettingsStore\n";
    return 0;
}
