#include "Rendering/CameraSettings.h"

#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }
}

int main()
{
    Tank::Rendering::CameraSettings source;
    source.position[0] = 3.0f;
    source.gazePoint[2] = 7.0f;
    source.up[0] = 0.25f;
    source.up[1] = 0.75f;
    source.projection = 1;
    source.followTank = true;
    source.lookDownDegrees = 42.0f;
    source.followYawOffsetDegrees = -35.0f;
    source.yawSpeedLimitDegrees = 120.0f;
    source.yawDamping = 6.0f;

    Tank::Rendering::CameraSettings loaded;
    std::string error;
    const bool passed =
        Tank::Rendering::DeserializeCameraSettings(
            Tank::Rendering::SerializeCameraSettings(source),
            loaded,
            &error) &&
        NearlyEqual(loaded.position[0], 3.0f) &&
        NearlyEqual(loaded.gazePoint[2], 7.0f) &&
        NearlyEqual(loaded.up[0], 0.25f) &&
        NearlyEqual(loaded.up[1], 0.75f) &&
        loaded.projection == 1 &&
        loaded.followTank &&
        NearlyEqual(loaded.lookDownDegrees, 42.0f) &&
        NearlyEqual(loaded.followYawOffsetDegrees, -35.0f) &&
        NearlyEqual(loaded.yawSpeedLimitDegrees, 120.0f) &&
        NearlyEqual(loaded.yawDamping, 6.0f);
    if (!passed)
    {
        std::cerr << "FAIL CameraSettings JSON: " << error << "\n";
        return 1;
    }
    std::cout << "PASS CameraSettings JSON\n";
    return 0;
}
