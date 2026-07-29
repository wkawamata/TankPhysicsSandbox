#include "Rendering/TankVisualSettingsJson.h"

#include <cmath>
#include <iostream>

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
            std::cerr << "FAIL TankVisualSettings JSON: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    Tank::Rendering::TankVisualSettings source;
    source.hullUpper.albedo = { 0.1f, 0.2f, 0.3f };
    source.hullUpper.roughness = 0.45f;
    source.hullUpper.metallic = 0.7f;
    source.hullUpper.ambientOcclusion = 0.8f;
    source.hullUpper.emissive = 0.25f;

    Tank::Rendering::TankVisualSettings loaded;
    std::string error;
    bool passed = Check(
        Tank::Rendering::DeserializeTankVisualSettings(
            Tank::Rendering::SerializeTankVisualSettings(source),
            loaded,
            &error),
        "serialized settings must deserialize");
    passed &= Check(NearlyEqual(loaded.hullUpper.albedo.r, 0.1f),
        "albedo must round trip");
    passed &= Check(NearlyEqual(loaded.hullUpper.roughness, 0.45f),
        "roughness must round trip");
    passed &= Check(NearlyEqual(loaded.hullUpper.metallic, 0.7f),
        "metallic must round trip");
    passed &= Check(NearlyEqual(loaded.hullUpper.ambientOcclusion, 0.8f),
        "ambient occlusion must round trip");
    passed &= Check(NearlyEqual(loaded.hullUpper.emissive, 0.25f),
        "emissive must round trip");

    const Tank::Rendering::TankVisualSettings beforeInvalid = loaded;
    passed &= Check(
        !Tank::Rendering::DeserializeTankVisualSettings("{invalid", loaded, &error),
        "invalid JSON must fail");
    passed &= Check(
        NearlyEqual(loaded.hullUpper.roughness, beforeInvalid.hullUpper.roughness),
        "invalid JSON must not modify settings");
    passed &= Check(
        !Tank::Rendering::DeserializeTankVisualSettings(
            R"({"version":999})",
            loaded,
            &error),
        "future version must fail");

    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS TankVisualSettings JSON\n";
    return 0;
}
