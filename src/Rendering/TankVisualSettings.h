#pragma once

namespace Tank::Rendering
{
    struct Color3
    {
        bool operator==(const Color3&) const = default;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    struct BodyMaterialSettings
    {
        bool operator==(const BodyMaterialSettings&) const = default;
        Color3 albedo = {};
        float roughness = 1.0f;
        float metallic = 0.0f;
        float ambientOcclusion = 1.0f;
        float emissive = 0.0f;
    };

    struct TankVisualSettings
    {
        bool operator==(const TankVisualSettings&) const = default;
        float gltfModelScale = 1.0f;
        // The glTF tank body is shown by default, so hide the overlapping dummy
        // body unless the user explicitly enables it.
        bool showDummyBody = false;
        bool showDummyWheels = true;
        bool showDummyTrackShoes = true;
        BodyMaterialSettings hullUpper = {
            { 0.28f, 0.48f, 0.32f }, 0.8f, 0.0f, 1.0f, 0.0f };
        BodyMaterialSettings hullLower = {
            { 0.16f, 0.28f, 0.19f }, 0.9f, 0.0f, 1.0f, 0.0f };
        BodyMaterialSettings structureUpper = {
            { 0.36f, 0.62f, 0.42f }, 0.7f, 0.0f, 1.0f, 0.0f };
        BodyMaterialSettings structureLower = {
            { 0.22f, 0.38f, 0.26f }, 0.85f, 0.0f, 1.0f, 0.0f };
        BodyMaterialSettings wheels = {
            { 0.16f, 0.17f, 0.18f }, 0.65f, 0.2f, 1.0f, 0.0f };
        bool colorWheelsByContact = false;
        BodyMaterialSettings contactedWheels = {
            { 0.27f, 0.78f, 0.35f }, 0.55f, 0.1f, 1.0f, 0.0f };
        BodyMaterialSettings trackShoes = {
            { 0.13f, 0.14f, 0.15f }, 0.75f, 0.35f, 1.0f, 0.0f };
        BodyMaterialSettings trackProxies = {
            { 0.16f, 0.16f, 0.18f }, 0.8f, 0.1f, 1.0f, 0.0f };
        BodyMaterialSettings forwardMarker = {
            { 1.0f, 0.24f, 0.24f }, 0.55f, 0.0f, 1.0f, 0.0f };
    };
}
