#pragma once

#include <string>

namespace Tank::Rendering
{
    struct CameraSettings
    {
        float position[3] = {};
        float gazePoint[3] = {};
        float up[3] = {0.0f, 1.0f, 0.0f};
        int projection = 0;
        float fovDegrees = 45.0f;
        float orthographicHeight = 10.0f;
        bool followTank = false;
        float followDistance = 16.0f;
        float lookDownDegrees = 25.0f;
        float followYawOffsetDegrees = 0.0f;
        float positionSpeed = 5.0f;
        float rotationSpeed = 8.0f;
        float damping = 1.0f;
        float yawSpeedLimitDegrees = 180.0f;
        float yawDamping = 8.0f;
    };

    std::string SerializeCameraSettings(const CameraSettings& settings);
    bool DeserializeCameraSettings(
        const std::string& json,
        CameraSettings& settings,
        std::string* error = nullptr);
}
