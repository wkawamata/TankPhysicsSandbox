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
    source.wheels.metallic = 0.9f;
    source.colorWheelsByContact = true;
    source.contactedWheels.albedo = { 0.2f, 0.8f, 0.3f };
    source.trackShoes.roughness = 0.35f;
    source.gltfModelScale = 0.75f;
    source.showDummyBody = false;
    source.showDummyWheels = false;
    source.showDummyTrackShoes = false;

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
    passed &= Check(NearlyEqual(loaded.wheels.metallic, 0.9f),
        "wheel material must round trip");
    passed &= Check(loaded.colorWheelsByContact,
        "wheel contact color toggle must round trip");
    passed &= Check(NearlyEqual(loaded.contactedWheels.albedo.g, 0.8f),
        "contacted wheel material must round trip");
    passed &= Check(NearlyEqual(loaded.trackShoes.roughness, 0.35f),
        "track shoe material must round trip");
    passed &= Check(NearlyEqual(loaded.gltfModelScale, 0.75f),
        "glTF model scale must round trip");
    passed &= Check(!loaded.showDummyBody && !loaded.showDummyWheels &&
        !loaded.showDummyTrackShoes,
        "dummy visibility must round trip");

    Tank::Rendering::TankVisualSettings versionOne;
    passed &= Check(
        Tank::Rendering::DeserializeTankVisualSettings(
            R"({"version":1,"hullUpper":{"roughness":0.25}})",
            versionOne,
            &error),
        "version 1 settings must remain readable");
    passed &= Check(NearlyEqual(versionOne.hullUpper.roughness, 0.25f),
        "version 1 material must load");
    passed &= Check(versionOne.showDummyBody && versionOne.showDummyWheels &&
        versionOne.showDummyTrackShoes && NearlyEqual(versionOne.gltfModelScale, 1.0f),
        "legacy settings must retain display defaults");

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
