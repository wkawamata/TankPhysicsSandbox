#pragma once

#include "Platform/IApplication.h"
#include "Platform/CommandLineOptions.h"
#include "Platform/WindowInfo.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>

#include "Engine/Rhi/Dx12/GraphicsDevice.h"
#include "Camera/DebugCameraController.h"
#include "Runtime/SceneRenderer.h"
#include "Runtime/SceneRendererDebugUi.h"
#include "Runtime/SceneRendererSettings.h"
#include "Renderer/EnvironmentMap.h"
#include "Ui/ImGuiSystem.h"
#include "Physics/BoxDropTest.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/TankVisualSettings.h"
#include "Platform/Windows/WindowsGamepad.h"
#include "Scene/SceneBuilder.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>

class TankSandboxApp : public Platform::IApplication
{
public:
    TankSandboxApp(UINT width, UINT height, std::wstring name);

    void ParseCommandLineArgs(WCHAR* argv[], int argc) override;

    void OnInit() override;
    void OnDestroy() override;
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
    };

    void InitializeImGui();
    void UpdateUiFrame();
    void DrawToolUi();
    void DrawCameraUi();
    void DrawRendererSettingsUi();
    void RequestScreenshot();
    void UpdateScreenshotResult();
    bool SaveRendererSettings();
    bool LoadRendererSettings();
    void ResetRendererSettings();
    void DrawTopMenuUi();
    void DrawPhysicsBoxDropUi();
    void DrawPhysicsTrackedVehicleUi();
    void EnterTrackedVehicleMode();
    void ResetTrackedVehicle();
    void ApplyTrackedVehicleBodyColors();
    bool SaveTankVisualSettings();
    bool LoadTankVisualSettings();
    bool SaveTankSettings();
    bool LoadTankSettings();
    bool SaveEnvironmentSettings();
    bool LoadEnvironmentSettings();
    void UpdateTrackedVehicleScene(const Tank::Physics::TrackedVehicleTestState& state);
    void UpdateTrackedVehicleInput();
    void ApplyTrackedVehicleCameraPreset(const DirectX::XMFLOAT3& offset);
    void ActivateOrbitCamera(Engine::Scene& scene, const DirectX::XMFLOAT3& pivot);
    void ApplyActiveCameraScene();
    Engine::CameraState* ActiveCamera();
    void EnterBoxDropMode();
    void UpdateBoxDropScene(const Tank::Physics::BoxDropState& state);
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

    // Box drop physics test and scene
    Tank::Physics::BoxDropTest m_boxDropTest;
    Engine::SceneBuilder m_boxDropSceneBuilder;
    size_t m_boxDropBoxInstanceIndex = 0;
    static constexpr float kPhysicsFixedDt = 1.0f / 60.0f;

    struct TrackedVehicleModel
    {
        static constexpr int kTrackShoeCountPerTrack = 32;

        size_t hullUpper = 0;
        size_t hullLower = 0;
        size_t upperStructureUpper = 0;
        size_t upperStructureLower = 0;
        size_t lowerStructureUpper = 0;
        size_t lowerStructureLower = 0;
        size_t leftTrack = 0;
        size_t rightTrack = 0;
        size_t forwardMarker = 0;
        std::array<size_t, Tank::Physics::kTankWheelCount> wheels = {};
        std::array<size_t, Tank::Physics::kTankWheelCount> suspensionLines = {};
        std::array<size_t, Tank::Physics::kTankWheelCount> contactMarkers = {};
        std::array<size_t, Tank::Physics::kTankWheelCount> contactNormalLines = {};
        std::array<std::array<size_t, kTrackShoeCountPerTrack>, Tank::Physics::kTankTrackCount>
            trackShoes = {};
        uint32_t wheelContactMaterial = 0;
        uint32_t wheelAirborneMaterial = 0;
        uint32_t debugContactMaterial = 0;
        uint32_t debugAirborneMaterial = 0;
        uint32_t hullUpperMaterial = 0;
        uint32_t hullLowerMaterial = 0;
        uint32_t structureUpperMaterial = 0;
        uint32_t structureLowerMaterial = 0;
    };
    Tank::Physics::TrackedVehicleTest m_trackedVehicleTest;
    Tank::Physics::TankSettings m_trackedVehicleSettings;
    Tank::Physics::TankSettings m_appliedTrackedVehicleSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_environmentSettings;
    Tank::Physics::PhysicsEnvironmentSettings m_appliedEnvironmentSettings;
    Tank::Platform::Windows::WindowsGamepad m_gamepad;
    Engine::SceneBuilder m_trackedVehicleSceneBuilder;
    TrackedVehicleModel m_trackedVehicleModel;
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_turnLeft = false;
    bool m_turnRight = false;
    bool m_pivotTurnModifier = false;
    bool m_rollLeft = false;
    bool m_rollRight = false;
    bool m_brake = false;
    bool m_analogTracksConnected = false;
    float m_analogLeftTrack = 0.0f;
    float m_analogRightTrack = 0.0f;
    float m_analogRoll = 0.0f;
    bool m_trackedVehiclePaused = false;
    bool m_trackedVehicleSingleStep = false;
    bool m_physicsDebugOverlay = false;
    bool m_trackShoeDisplay = true;
    bool m_showTrackProxies = false;
    Tank::Rendering::TankVisualSettings m_tankVisualSettings;
    std::array<float, Tank::Physics::kTankTrackCount> m_trackShoeDistances = {};
    float m_trackShoeLastTimeSeconds = 0.0f;
    bool m_rendererDebugOpen = true;
    RtPbrSurvey::SceneRendererSettings m_defaultRendererSettings;
    Engine::ProceduralEnvironmentSettings m_rendererEnvironmentSettings;
    bool m_rendererEnvironmentAutoUpdate = Engine::kUseGpuProceduralEnvMap;
    bool m_rendererEnvironmentReloadPending = false;
    std::string m_rendererSettingsStatus;
    std::string m_tankSettingsStatus;
    std::string m_tankVisualSettingsStatus;
    int m_tankSettingsSlot = 0;
    std::string m_environmentSettingsStatus;
    std::string m_screenshotStatus;

    // Auto scene entry and screenshot for CLI.
    std::optional<AppMode> m_autoSceneMode;
    UINT64 m_autoCaptureFrameCount = 0;
    UINT64 m_autoFramesElapsed = 0;
    bool m_quitAfterCapture = false;

    // Debug logging to file (-LogToFile).
    ComPtr<ID3D12InfoQueue> m_d3d12InfoQueue;
    FILE* m_logFile = nullptr;
    UINT64 m_fpsLogFrameCounter = 0;
};
