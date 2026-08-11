#pragma once

#include <functional>

namespace DirectX { struct XMFLOAT3; }
namespace Engine { struct CameraState; class Scene; }
namespace Tank::App { class CameraController; }
namespace Tank::Physics { struct TrackedVehicleTestState; }

namespace Ui
{
    struct CameraPanelContext
    {
        Tank::App::CameraController* cameraController = nullptr;
        Engine::CameraState* camera = nullptr;
        bool trackedVehicleActive = false;
        const Tank::Physics::TrackedVehicleTestState* vehicleState = nullptr;
        Engine::Scene* vehicleScene = nullptr;

        std::function<void(const Engine::CameraState&)> setCamera;
        std::function<void(Engine::Scene&, const DirectX::XMFLOAT3&)> activateOrbitCamera;
        std::function<void()> saveCamera;
        std::function<void()> loadCamera;

        bool telemetryWasTransitioning = false;
        bool telemetryHasSample = false;
        float telemetryMinimumPositionY = 0.0f;
        float telemetryMinimumFocusDistance = 0.0f;
    };

    void DrawCameraPanel(CameraPanelContext& ctx);
}
