#pragma once
#include <Scene/Scene.h>
#include <algorithm>
#include <cmath>

namespace Tank::App
{
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
