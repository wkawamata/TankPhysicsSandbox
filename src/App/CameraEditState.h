#pragma once
#include "Rendering/CameraSettings.h"
#include <array>
namespace Tank::App
{
    struct CameraEditState
    {
        Rendering::CameraSettings settings;
        std::array<float, 3> rotation{};
        float nearZ = 0.1f;
        float farZ = 10000;
        float mouseSpeed = 1;
        int slot = 0;
        int mouseMode = 0;
        bool autoLoad = true;
        bool yawChase = true;
        bool operator==(const CameraEditState&) const = default;
    };
}
