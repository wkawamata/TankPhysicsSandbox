#pragma once

#include <array>
#include <string>
#include <vector>

namespace Tank::Map
{
    struct Transform
    {
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotationDegrees = { 0.0f, 0.0f, 0.0f };
    };

    struct Instance
    {
        std::string id;
        // UTF-8 path relative to the map folder, using forward slashes.
        std::string asset;
        Transform transform;
    };

    struct ClearArea
    {
        std::string id;
        std::string name;
        std::array<float, 3> center = { 0.0f, 0.0f, 0.0f };
        // Full extents in meters, along world axes.
        std::array<float, 3> size = { 1.0f, 1.0f, 1.0f };
    };

    struct Manifest
    {
        Transform playerSpawn = { { 0.0f, 2.0f, 0.0f }, {} };
        std::vector<Instance> instances;
        std::vector<ClearArea> clearAreas;
    };

    // Failed operations leave the output unchanged and provide an error.
    bool SerializeManifest(const Manifest& manifest, std::string& jsonText, std::string& error);
    bool DeserializeManifest(const std::string& jsonText, Manifest& manifest, std::string& error);
}
