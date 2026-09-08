#pragma once

#include "Platform/IApplication.h"
#include "Platform/CommandLineOptions.h"
#include "Platform/WindowInfo.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>

#include "App/BoxDropMode.h"
#include "App/MapEditorMode.h"
#include "App/CameraController.h"
#include "App/CameraSettingsStore.h"
#include "App/TankSettingsStore.h"
#include "App/TankVisualSettingsStore.h"
#include "App/TrackedVehicleMode.h"
#include "Engine/Rhi/Dx12/GraphicsDevice.h"
#include "Camera/DebugCameraController.h"
#include "Runtime/SceneRenderer.h"
#include "Runtime/SceneRendererDebugUi.h"
#include "Runtime/SceneRendererSettings.h"
#include "Rendering/CameraSettings.h"
#include "Rendering/MapEditorScenePresenter.h"
#include "Ui/ImGuiSystem.h"
#include "Ui/CameraPanel.h"
#include "Ui/RendererSettingsPanel.h"
#include "Ui/TrackedVehiclePanel.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/MapDefinition.h"
#include "Physics/MapDefinitionJson.h"
#include "Rendering/TankVisualSettings.h"
#include "Platform/Windows/WindowsGamepad.h"
#include "Scene/SceneBuilder.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class TankSandboxApp : public Platform::IApplication
{
public:
    TankSandboxApp(UINT width, UINT height, std::wstring name);

    void ParseCommandLineArgs(WCHAR* argv[], int argc) override;

    void OnInit() override;
    void OnDestroy() override;
    bool OnCloseRequested() override;
    void OnKeyDown(UINT8 key) override;
    void OnKeyUp(UINT8 key) override;
    void OnMouseDown(UINT8 button, int x, int y) override;
    void OnMouseUp(UINT8 button, int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseWheel(int wheelDelta) override;
    void OnWindowSizeChanged(UINT width, UINT height) override;
    void OnIdle() override;

    UINT GetWidth() const override { return m_windowInfo.width; }
    UINT GetHeight() const override { return m_windowInfo.height; }
    const WCHAR* GetTitle() const override { return m_windowInfo.title.c_str(); }

private:
    enum class AppMode
    {
        TopMenu,
        PhysicsBoxDrop,
        PhysicsTrackedVehicle,
        MapEditor,
    };

    void InitializeImGui();
    void UpdateUiFrame();
    void DrawToolUi();
    void RequestScreenshot();
    void UpdateScreenshotResult();
    bool SaveRendererSettings();
    bool LoadRendererSettings();
    void ResetRendererSettings();
    bool SaveCameraSettings();
    bool LoadCameraSettings();
    void DrawTopMenuUi();
    void ReloadCustomMaps();
    bool LoadAutoMap();
    void EnterTrackedVehicleMode();
    void EnterBoxDropMode();
    void ActivateOrbitCamera(Engine::Scene& scene, const DirectX::XMFLOAT3& pivot);
    void ApplyActiveCameraScene();
    Engine::CameraState* ActiveCamera();
    Engine::Scene* ActiveScene();
    bool EnsureDebugCameraForMouse();
    bool HasInputFocus() const;
    void ClearVehicleInputState();
    void FlushD3d12DebugLog();
    void LogFps(float cpuFrameTimeMs);

    static constexpr DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    static constexpr UINT kImGuiDescriptorCount = 100;

    Platform::WindowInfo m_windowInfo;
    Platform::CommandLineOptions m_commandLineOptions;
    GraphicsDevice m_graphicsDevice;
    ComPtr<ID3D12DescriptorHeap> m_imguiHeap;
    Engine::ImGuiSystem m_imguiSystem;
    RtPbrSurvey::SceneRenderer m_sceneRenderer;
    RtPbrSurvey::DebugCameraController m_debugCameraController;
    AppMode m_appMode = AppMode::TopMenu;
    Tank::Physics::MapId m_selectedMap =
        Tank::Physics::MapId::ObstacleField;
    struct CustomMapEntry
    {
        std::string fileName;
        Tank::Physics::MapDocument document;
    };
    std::vector<CustomMapEntry> m_customMaps;
    std::optional<size_t> m_selectedCustomMap;
    std::string m_customMapStatus;

    // Mode state
    BoxDropMode m_boxDropMode;
    MapEditorMode m_mapEditorMode;
    Tank::Rendering::MapEditorScenePresenter m_mapEditorScenePresenter;
    TrackedVehicleMode m_trackedVehicleMode;

    // Platform input (owned here to avoid mixing platform input with physics mode)
    Tank::Platform::Windows::WindowsGamepad m_gamepad;
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_turnLeft = false;
    bool m_turnRight = false;
    bool m_pivotTurnModifier = false;
    bool m_rollLeft = false;
    bool m_rollRight = false;
    bool m_brake = false;
    // Camera
    Tank::App::CameraController m_cameraController;

    // UI panel contexts
    Ui::CameraPanelContext m_cameraPanelCtx;
    Ui::RendererSettingsPanelContext m_rendererPanelCtx;
    Ui::TrackedVehiclePanelContext m_trackedVehiclePanelCtx;

    // Renderer state
    bool m_rendererDebugOpen = true;
    RtPbrSurvey::SceneRendererSettings m_defaultRendererSettings;
    RtPbrSurvey::EnvironmentMappingUiState m_environmentMappingUi;
    std::string m_rendererSettingsStatus;
    std::string m_screenshotStatus;

    // Auto scene entry and screenshot for CLI.
    std::optional<AppMode> m_autoSceneMode;
    std::optional<std::filesystem::path> m_autoMapPath;
    UINT64 m_autoCaptureFrameCount = 0;
    UINT64 m_autoFramesElapsed = 0;
    bool m_quitAfterCapture = false;
    bool m_windowCloseApproved = false;

    // Debug logging to file (-LogToFile).
    ComPtr<ID3D12InfoQueue> m_d3d12InfoQueue;
    FILE* m_logFile = nullptr;
    UINT64 m_fpsLogFrameCounter = 0;
};
