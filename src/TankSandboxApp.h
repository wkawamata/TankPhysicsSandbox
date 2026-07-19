#pragma once

#include "Platform/IApplication.h"
#include "Platform/CommandLineOptions.h"
#include "Platform/WindowInfo.h"
#include "Engine/Rhi/Dx12/GraphicsDevice.h"
#include "Runtime/SceneRenderer.h"
#include "Ui/ImGuiSystem.h"
#include "Physics/BoxDropTest.h"
#include "Physics/TrackedVehicleTest.h"
#include "Scene/SceneBuilder.h"

#include <d3d12sdklayers.h>
#include <chrono>

class TankSandboxApp : public Platform::IApplication
{
public:
    TankSandboxApp(UINT width, UINT height, std::wstring name);

    void ParseCommandLineArgs(WCHAR* argv[], int argc) override;

    void OnInit() override;
    void OnDestroy() override;
    void OnKeyDown(UINT8 key) override;
    void OnKeyUp(UINT8 key) override;
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
    void DrawTopMenuUi();
    void DrawPhysicsBoxDropUi();
    void DrawPhysicsTrackedVehicleUi();
    void EnterTrackedVehicleMode();
    void UpdateTrackedVehicleScene(const Tank::Physics::TrackedVehicleTestState& state);
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
    AppMode m_appMode = AppMode::TopMenu;

    // Box drop physics test and scene
    Tank::Physics::BoxDropTest m_boxDropTest;
    Engine::SceneBuilder m_boxDropSceneBuilder;
    size_t m_boxDropBoxInstanceIndex = 0;
    static constexpr float kPhysicsFixedDt = 1.0f / 60.0f;

    Tank::Physics::TrackedVehicleTest m_trackedVehicleTest;
    Engine::SceneBuilder m_trackedVehicleSceneBuilder;
    size_t m_trackedVehicleBodyInstanceIndex = 0;

    // Debug logging to file (-LogToFile).
    ComPtr<ID3D12InfoQueue> m_d3d12InfoQueue;
    FILE* m_logFile = nullptr;
    UINT64 m_fpsLogFrameCounter = 0;
};
