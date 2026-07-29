#pragma once

namespace Tank::Rendering
{
    struct Color3
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    struct BodyMaterialSettings
    {
        Color3 albedo = {};
        float roughness = 1.0f;
        float metallic = 0.0f;
        float ambientOcclusion = 1.0f;
        float emissive = 0.0f;
    };

    struct TankVisualSettings
    {
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
        BodyMaterialSettings trackShoes = {
            { 0.13f, 0.14f, 0.15f }, 0.75f, 0.35f, 1.0f, 0.0f };
        BodyMaterialSettings trackProxies = {
            { 0.16f, 0.16f, 0.18f }, 0.8f, 0.1f, 1.0f, 0.0f };
        BodyMaterialSettings forwardMarker = {
            { 1.0f, 0.24f, 0.24f }, 0.55f, 0.0f, 1.0f, 0.0f };
    };
}
