#pragma once

#include <functional>
#include "App/CameraEditState.h"
namespace Tank::App { class TankSettingsUndo; }

namespace DirectX { struct XMFLOAT3; }
namespace Engine { struct CameraState; class Scene; }
namespace Tank::App { class CameraController; }
namespace RtPbrSurvey { class DebugCameraController; }
namespace Tank::Physics { struct TrackedVehicleTestState; }

namespace Ui
{
    struct CameraPanelContext
    {
        Tank::App::TankSettingsUndo* settingsUndo = nullptr;
        Tank::App::CameraController* cameraController = nullptr;
        Engine::CameraState* camera = nullptr;
        bool trackedVehicleActive = false;
        const Tank::Physics::TrackedVehicleTestState* vehicleState = nullptr;
        Engine::Scene* vehicleScene = nullptr;

        std::function<void(const Engine::CameraState&)> setCamera;
        std::function<void(Engine::Scene&, const DirectX::XMFLOAT3&)> activateOrbitCamera;
        std::function<void()> cameraRestored;
        std::function<void()> saveCamera;
        std::function<void()> loadCamera;
        std::function<void()> drawGameViewportControls;

        bool telemetryWasTransitioning = false;
        bool mortarCheatWindowVisible = false;
        bool mortarCheatWindowJapanese = true;
        float mortarCheatFontScale = 1.0f;
        bool telemetryHasSample = false;
        float telemetryMinimumPositionY = 0.0f;
        float telemetryMinimumFocusDistance = 0.0f;
    };

    void DrawCameraPanel(CameraPanelContext& ctx);
    Tank::App::CameraEditState CaptureEditorCamera(const Engine::CameraState& camera,
        const RtPbrSurvey::DebugCameraController& controller);
    void RestoreEditorCamera(const Tank::App::CameraEditState& state,
        Engine::CameraState& camera, RtPbrSurvey::DebugCameraController& controller);
    bool DrawMapEditorCameraWindow(Engine::CameraState& camera,
        RtPbrSurvey::DebugCameraController& controller, bool& open,
        const std::function<void()>& focusSelected = {},
        const std::function<void(const Tank::App::CameraEditState&, const Tank::App::CameraEditState&)>& recordEdit = {});
}
