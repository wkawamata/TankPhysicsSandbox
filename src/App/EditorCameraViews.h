#pragma once
#include <Scene/Scene.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace Tank::App
{
    inline void FocusEditorCamera(Engine::CameraState& camera, const std::array<float, 3>& center,
        float radius, float aspectRatio)
    {
        float dx = camera.pos.x-camera.gazePoint.x, dy = camera.pos.y-camera.gazePoint.y, dz = camera.pos.z-camera.gazePoint.z;
        float length = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (length < 0.0001f) {dx = 1; dy = 0.75f; dz = -1; length = std::sqrt(dx*dx + dy*dy + dz*dz);}
        const float safeRadius = (std::max)(radius, 0.1f);
        const float aspect = (std::max)(aspectRatio, 0.01f);
        const float halfVerticalFov = DirectX::XMConvertToRadians(std::clamp(camera.fov, 1.0f, 179.0f)) * 0.5f;
        const float halfFov = (std::min)(halfVerticalFov, std::atan(std::tan(halfVerticalFov) * aspect));
        // Fit the enclosing sphere within the central 50% of the limiting screen dimension.
        constexpr float screenFraction = 0.5f;
        const float objectHalfAngle = std::atan(screenFraction * std::tan(halfFov));
        const float distance = (std::max)(camera.nearZ + safeRadius, safeRadius / std::sin(objectHalfAngle));
        if (camera.projection == Engine::CameraProjection::Orthographic)
            camera.orthographicHeight = (std::max)(1.0f, 2.0f * safeRadius / (screenFraction * (std::min)(1.0f, aspect)));
        camera.gazePoint = {center[0], center[1], center[2]};
        camera.pos = {center[0] + dx*distance/length, center[1] + dy*distance/length, center[2] + dz*distance/length};
    }

    enum class EditorOrthoPlane { XZ, XY, YZ };
    inline void ApplyEditorOrthoPlane(Engine::CameraState& camera, EditorOrthoPlane plane)
    {
        const float dx = camera.pos.x - camera.gazePoint.x;
        const float dy = camera.pos.y - camera.gazePoint.y;
        const float dz = camera.pos.z - camera.gazePoint.z;
        const float distance = (std::max)(0.1f, std::sqrt(dx * dx + dy * dy + dz * dz));
        if (camera.projection == Engine::CameraProjection::Perspective)
            camera.orthographicHeight = std::clamp(2 * distance * std::tan(DirectX::XMConvertToRadians(camera.fov) * 0.5f), 1.0f, 1000.0f);
        camera.projection = Engine::CameraProjection::Orthographic;
        camera.pos = camera.gazePoint;
        camera.up = {0, 1, 0};
        if (plane == EditorOrthoPlane::XZ) {camera.pos.y += distance; camera.up = {0, 0, 1};}
        else if (plane == EditorOrthoPlane::XY) camera.pos.z -= distance;
        else camera.pos.x += distance;
    }
}
