#include "TankSandboxApp.h"
#include "Input/TankInputMapper.h"
#include "Platform/Win32Application.h"
#include "Scene/SceneBuilder.h"
#include "imgui.h"

#include <Camera/DebugCameraController.h>
#include <Engine/Rhi/Dx12/GraphicsDevice.h>
#include <Engine/RtPbrSurveyEngine.h>
#include <GltfLoader.h>
#include <Platform/CommandLineOptions.h>
#include <Platform/WindowInfo.h>
#include <Runtime/SceneRendererDebugUi.h>
#include <Runtime/SceneRendererSettings.h>
#include <Scene/Scene.h>
#include <Shared/Error.h>
#include <Shared/Screenshot.h>
#include <ImGuiWidgets.h>

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>
#include <Windows.h>
#include <combaseapi.h>
#include <corecrt.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <fcntl.h>
#include <io.h>
#include <libloaderapi.h>
#include <minwinbase.h>
#include <share.h>
#include <sys/stat.h>
#include <sysinfoapi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "Physics/TankTypes.h"
#include "Physics/TestObstacleLayout.h"
#include "Rendering/CameraSettings.h"
#include "Input/GamepadState.h"

using namespace DirectX;

namespace
{
    constexpr const char* kRendererSettingsPath = "Config/renderer_debug.json";
}

TankSandboxApp::TankSandboxApp(UINT width, UINT height, std::wstring name)
    : m_windowInfo(Platform::CreateWindowInfo(width, height, name))
    , m_sceneRenderer(m_graphicsDevice)
{
}

void TankSandboxApp::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    m_commandLineOptions = Platform::ParseCommandLineOptions(argv, argc);
    if (m_commandLineOptions.useWarpDevice)
    {
        m_windowInfo.title += L" (WARP)";
    }

    for (int i = 1; i < argc; i++)
    {
        const std::wstring arg(argv[i]);

        if (arg == L"--scene" && i + 1 < argc)
        {
            const std::wstring scene(argv[i + 1]);
            if (scene == L"box-drop")
            {
                m_autoSceneMode = AppMode::PhysicsBoxDrop;
            }
            else if (scene == L"tracked-vehicle")
            {
                m_autoSceneMode = AppMode::PhysicsTrackedVehicle;
            }
            i++;
        }
        else if (arg == L"--capture-after-frames" && i + 1 < argc)
        {
            m_autoCaptureFrameCount = _wtoi64(argv[i + 1]);
            i++;
        }
        else if (arg == L"--quit-after-capture")
        {
            m_quitAfterCapture = true;
        }
        else if (arg == L"--physics-debug-overlay")
        {
            m_trackedVehicleMode.SetPhysicsDebugOverlayDefault(true);
        }
    }
}

void TankSandboxApp::OnInit()
{
    GraphicsDeviceDesc deviceDesc = {};
    deviceDesc.hwnd = Win32Application::GetHwnd();
    deviceDesc.swapChainWidth = GetWidth();
    deviceDesc.swapChainHeight = GetHeight();
    deviceDesc.bufferCount = 2;
    deviceDesc.swapChainFormat = kSwapChainFormat;
    deviceDesc.useWarpDevice = m_commandLineOptions.useWarpDevice;
    m_graphicsDevice.Initialize(deviceDesc);

    // Open debug log file and query ID3D12InfoQueue for D3D12 message capture.
    if (!m_commandLineOptions.logFilePath.empty())
    {
        int fd = -1;
        errno_t err = _wsopen_s(&fd, m_commandLineOptions.logFilePath.c_str(),
            _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT,
            _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (err == 0 && fd != -1)
        {
            m_logFile = _fdopen(fd, "wt");
        }
        if (m_logFile)
        {
            fprintf(m_logFile, "[STATUS] Log file opened at %ls\n", m_commandLineOptions.logFilePath.c_str());
            fflush(m_logFile);
            m_graphicsDevice.Device()->QueryInterface(IID_PPV_ARGS(&m_d3d12InfoQueue));
            if (m_d3d12InfoQueue)
            {
                fprintf(m_logFile, "[STATUS] ID3D12InfoQueue obtained successfully\n");
                m_d3d12InfoQueue->SetMessageCountLimit(100000);
            }
            else
            {
                fprintf(m_logFile, "[STATUS] ID3D12InfoQueue QueryInterface failed\n");
            }
            fflush(m_logFile);
        }
    }

    InitializeImGui();

    m_sceneRenderer.Initialize(GetWidth(), GetHeight());

    m_sceneRenderer.SetToolUiHandler([this]() { DrawToolUi(); });

    Engine::SceneBuilder builder;
    // SceneRenderer currently requires non-empty geometry when resources are loaded.
    const uint32_t bootstrapMaterial = builder.AddSolidColorMaterial(0, 0, 0, 0);
    builder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    builder.AddInstance(XMMatrixIdentity(), bootstrapMaterial);

    // Camera looking at the cube from close range.
    Engine::CameraState camera;
    camera.pos = { 0.0f, 0.0f, -3.0f };
    camera.gazePoint = { 0.0f, 0.0f, 0.0f };
    camera.fov = 60.0f;
    camera.nearZ = 0.1f;
    camera.farZ = 10000.0f;
    builder.SetCamera(camera);

    m_sceneRenderer.SetBackBufferClearColor({ 0.53f, 0.74f, 0.95f, 1.0f });

    // Disable skybox, boost IBL.
    RtPbrSurveyEngine::LightingParams lighting;
    lighting.skyboxEnabled = false;
    lighting.iblIntensity = 1.0f;
    m_sceneRenderer.SetLightingParams(lighting);

    // Temporarily disable shadows while the host debug UI and shadow artifacts are investigated.
    auto shadowSettings = m_sceneRenderer.GetShadowSettings();
    shadowSettings.enabled = false;
    shadowSettings.normalBias = 0.05f;
    m_sceneRenderer.SetShadowSettings(shadowSettings);

    m_sceneRenderer.SetScene(builder.GetScene());
    m_sceneRenderer.ReloadSceneResources(builder.GetScene());
    m_sceneRenderer.SetDisplayInstanceCount(0);

    m_defaultRendererSettings = m_sceneRenderer.CaptureSettings();
    LoadRendererSettings();
    m_environmentMappingUi.lighting = m_sceneRenderer.GetLightingParams();
    m_environmentMappingUi.iblEnabled =
        m_environmentMappingUi.lighting.diffuseIblEnabled ||
        m_environmentMappingUi.lighting.specularIblEnabled;
    if (m_trackedVehicleMode.TankSettingsAutoLoad())
    {
        m_trackedVehicleMode.LoadTankSettings(false, m_sceneRenderer, m_cameraController);
    }
    if (m_trackedVehicleMode.TankVisualSettingsAutoLoad())
    {
        m_trackedVehicleMode.LoadTankVisualSettings(false, m_sceneRenderer);
    }
    m_gamepad.Initialize();

    m_cameraPanelCtx.cameraController = &m_cameraController;
    m_cameraPanelCtx.setCamera = [this](const Engine::CameraState& c) { m_sceneRenderer.SetCamera(c); };
    m_cameraPanelCtx.activateOrbitCamera = [this](Engine::Scene& scene, const DirectX::XMFLOAT3& pivot)
        { ActivateOrbitCamera(scene, pivot); };
    m_cameraPanelCtx.saveCamera = [this]() { SaveCameraSettings(); };
    m_cameraPanelCtx.loadCamera = [this]() { LoadCameraSettings(); };

    m_rendererPanelCtx.settingsStatus = &m_rendererSettingsStatus;
    m_rendererPanelCtx.screenshotStatus = &m_screenshotStatus;
    m_rendererPanelCtx.saveSettings = [this]() { SaveRendererSettings(); };
    m_rendererPanelCtx.loadSettings = [this]() { LoadRendererSettings(); };
    m_rendererPanelCtx.resetSettings = [this]() { ResetRendererSettings(); };
    m_rendererPanelCtx.requestScreenshot = [this]() { RequestScreenshot(); };

    m_trackedVehiclePanelCtx.state = &m_trackedVehicleMode.Test().State();
    m_trackedVehiclePanelCtx.driverInput = &m_trackedVehicleMode.Test().DriverInput();
    m_trackedVehiclePanelCtx.physicsDebugOverlay = &m_trackedVehicleMode.PhysicsDebugOverlay();
    m_trackedVehiclePanelCtx.trackShoeDisplay = &m_trackedVehicleMode.TrackShoeDisplay();
    m_trackedVehiclePanelCtx.showTrackProxies = &m_trackedVehicleMode.ShowTrackProxies();
    m_trackedVehiclePanelCtx.trackedVehiclePaused = &m_trackedVehicleMode.Paused();
    m_trackedVehiclePanelCtx.trackedVehicleSingleStep = &m_trackedVehicleMode.SingleStep();
    m_trackedVehiclePanelCtx.tankSettingsSlot = &m_trackedVehicleMode.TankSettingsSlot();
    m_trackedVehiclePanelCtx.tankSettingsAutoLoad = &m_trackedVehicleMode.TankSettingsAutoLoad();
    m_trackedVehiclePanelCtx.tankVisualSettingsAutoLoad = &m_trackedVehicleMode.TankVisualSettingsAutoLoad();
    m_trackedVehiclePanelCtx.tankVisualMaterialApplyPending = &m_trackedVehicleMode.TankVisualMaterialApplyPending();
    m_trackedVehiclePanelCtx.tankSettingsStatus = &m_trackedVehicleMode.TankSettingsStatus();
    m_trackedVehiclePanelCtx.tankVisualSettingsStatus = &m_trackedVehicleMode.TankVisualSettingsStatus();
    m_trackedVehiclePanelCtx.envSettingsStatus = &m_trackedVehicleMode.EnvSettingsStatus();
    m_trackedVehiclePanelCtx.tankSettings = &m_trackedVehicleMode.Settings();
    m_trackedVehiclePanelCtx.appliedTankSettings = &m_trackedVehicleMode.AppliedSettings();
    m_trackedVehiclePanelCtx.envSettings = &m_trackedVehicleMode.EnvSettings();
    m_trackedVehiclePanelCtx.appliedEnvSettings = &m_trackedVehicleMode.AppliedEnvSettings();
    m_trackedVehiclePanelCtx.visualSettings = &m_trackedVehicleMode.VisualSettings();
    m_trackedVehiclePanelCtx.updateScene = [this]()
    {
        m_trackedVehicleMode.UpdateScene(m_sceneRenderer);
    };
    m_trackedVehiclePanelCtx.enterTrackedVehicleMode = [this]() { EnterTrackedVehicleMode(); };
    m_trackedVehiclePanelCtx.resetTrackedVehicle = [this]()
    {
        m_trackedVehicleMode.Reset(m_sceneRenderer, m_cameraController);
    };
    m_trackedVehiclePanelCtx.applyMaterials = [this]()
    {
        m_trackedVehicleMode.ApplyMaterials(m_sceneRenderer);
    };
    m_trackedVehiclePanelCtx.saveTankSettings = [this]()
    {
        m_trackedVehicleMode.SaveTankSettings();
    };
    m_trackedVehiclePanelCtx.loadTankSettings = [this]()
    {
        m_trackedVehicleMode.LoadTankSettings(true, m_sceneRenderer, m_cameraController);
    };
    m_trackedVehiclePanelCtx.saveTankVisualSettings = [this]()
    {
        m_trackedVehicleMode.SaveTankVisualSettings();
    };
    m_trackedVehiclePanelCtx.loadTankVisualSettings = [this]()
    {
        m_trackedVehicleMode.LoadTankVisualSettings(true, m_sceneRenderer);
    };
    m_trackedVehiclePanelCtx.saveEnvSettings = [this]()
    {
        m_trackedVehicleMode.SaveEnvironmentSettings();
    };
    m_trackedVehiclePanelCtx.loadEnvSettings = [this]()
    {
        if (m_trackedVehicleMode.LoadEnvironmentSettings())
        {
            EnterTrackedVehicleMode();
        }
    };

    if (m_autoSceneMode.has_value())
    {
        switch (*m_autoSceneMode)
        {
        case AppMode::PhysicsBoxDrop:
            EnterBoxDropMode();
            break;
        case AppMode::PhysicsTrackedVehicle:
            EnterTrackedVehicleMode();
            break;
        }
    }
}

void TankSandboxApp::OnDestroy()
{
    m_sceneRenderer.Shutdown();
    if (m_logFile)
    {
        FlushD3d12DebugLog();
        fclose(m_logFile);
        m_logFile = nullptr;
    }
    m_d3d12InfoQueue.Reset();
}

void TankSandboxApp::OnKeyDown(UINT8 key)
{
    if (key == VK_F12)
    {
        RequestScreenshot();
    }
    else if (key == VK_ESCAPE)
    {
        if (m_appMode != AppMode::TopMenu)
        {
            m_appMode = AppMode::TopMenu;
            m_boxDropMode.Exit();
            m_trackedVehicleMode.Exit();
            m_sceneRenderer.SetScene(Engine::Scene{});
        }
        else
        {
            PostQuitMessage(0);
        }
    }
    else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'R')
    {
        m_trackedVehicleMode.Reset(m_sceneRenderer, m_cameraController);
    }
    else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'P')
    {
        m_trackedVehicleMode.Paused() = !m_trackedVehicleMode.Paused();
    }
    else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'N' && m_trackedVehicleMode.Paused())
    {
        m_trackedVehicleMode.SingleStep() = true;
    }
    else if (key == 'W') m_moveForward = true;
    else if (key == 'S') m_moveBackward = true;
    else if (key == 'A') m_turnRight = true;
    else if (key == 'D') m_turnLeft = true;
    else if (key == 'Q') m_rollLeft = true;
    else if (key == 'E') m_rollRight = true;
    else if (key == VK_SHIFT) m_pivotTurnModifier = true;
    else if (key == VK_SPACE) m_brake = true;
}

void TankSandboxApp::OnKeyUp(UINT8 key)
{
    if (key == 'W') m_moveForward = false;
    else if (key == 'S') m_moveBackward = false;
    else if (key == 'A') m_turnRight = false;
    else if (key == 'D') m_turnLeft = false;
    else if (key == 'Q') m_rollLeft = false;
    else if (key == 'E') m_rollRight = false;
    else if (key == VK_SHIFT) m_pivotTurnModifier = false;
    else if (key == VK_SPACE) m_brake = false;
}

void TankSandboxApp::OnMouseDown(UINT8 button, int x, int y)
{
    if (m_appMode != AppMode::TopMenu &&
        !(m_appMode == AppMode::PhysicsTrackedVehicle && m_cameraController.FollowEnabled()))
    {
        m_debugCameraController.OnMouseDown(button, x, y);
    }
}

void TankSandboxApp::OnMouseUp(UINT8 button, int x, int y)
{
    if (m_appMode != AppMode::TopMenu &&
        !(m_appMode == AppMode::PhysicsTrackedVehicle && m_cameraController.FollowEnabled()))
    {
        m_debugCameraController.OnMouseUp(button, x, y);
        ApplyActiveCameraScene();
    }
}

void TankSandboxApp::OnMouseMove(int x, int y)
{
    if (m_appMode != AppMode::TopMenu &&
        !(m_appMode == AppMode::PhysicsTrackedVehicle && m_cameraController.FollowEnabled()))
    {
        m_debugCameraController.OnMouseMove(x, y);
        ApplyActiveCameraScene();
    }
}

void TankSandboxApp::OnMouseWheel(int wheelDelta)
{
    if (m_appMode != AppMode::TopMenu &&
        !(m_appMode == AppMode::PhysicsTrackedVehicle && m_cameraController.FollowEnabled()))
    {
        m_debugCameraController.OnMouseWheel(wheelDelta, false);
        ApplyActiveCameraScene();
    }
}

void TankSandboxApp::OnWindowSizeChanged(UINT width, UINT height)
{
    m_windowInfo.width = width;
    m_windowInfo.height = height;
    m_windowInfo.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    m_debugCameraController.SetWindowSize(width, height);
    m_sceneRenderer.RequestResize(width, height);
}

void TankSandboxApp::OnIdle()
{
    if (m_appMode == AppMode::PhysicsBoxDrop)
    {
        m_boxDropMode.Update(m_sceneRenderer);
    }
    else if (m_appMode == AppMode::PhysicsTrackedVehicle)
    {
        m_gamepad.Poll();
        {
            const Tank::Input::GamepadState& gp = m_gamepad.State();
            m_cameraController.UpdateButtonStates(
                gp.connected && gp.buttonCount > 4 && gp.rawButtons[4],
                gp.connected && gp.buttonCount > 7 && gp.rawButtons[7]);
        }
        m_trackedVehicleMode.UpdateInput(
            m_gamepad.State(),
            m_moveForward, m_moveBackward,
            m_turnLeft, m_turnRight, m_pivotTurnModifier,
            m_rollLeft, m_rollRight, m_brake);
        m_trackedVehicleMode.Step(m_sceneRenderer, m_cameraController);
    }
    if (Engine::CameraState* camera = ActiveCamera())
    {
        m_cameraController.UpdateTransition(TrackedVehicleMode::kPhysicsFixedDt, *camera);
        m_cameraController.UpdateSlotCache(*camera);
    }

    UpdateUiFrame();

    m_sceneRenderer.RunFrame(
        [this](ID3D12GraphicsCommandList* commandList)
        {
            m_imguiSystem.Render(commandList);
        });
    UpdateScreenshotResult();

    if (m_autoCaptureFrameCount > 0)
    {
        m_autoFramesElapsed++;
        if (m_autoFramesElapsed >= m_autoCaptureFrameCount)
        {
            RequestScreenshot();
            m_autoCaptureFrameCount = 0;
        }
    }

    if (m_quitAfterCapture && m_autoCaptureFrameCount == 0 &&
        m_screenshotStatus.find("Saved: ") == 0)
    {
        PostQuitMessage(0);
    }

    if (m_logFile)
    {
        FlushD3d12DebugLog();
        LogFps(m_sceneRenderer.CpuFrameTimeMs());
    }
}

void TankSandboxApp::FlushD3d12DebugLog()
{
    if (!m_logFile)
    {
        return;
    }

    if (!m_d3d12InfoQueue)
    {
        fprintf(m_logFile, "[STATUS] ID3D12InfoQueue is NULL - no D3D12 debug messages available\n");
        fflush(m_logFile);
        return;
    }

    const UINT64 count = m_d3d12InfoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < count; i++)
    {
        SIZE_T len = 0;
        m_d3d12InfoQueue->GetMessage(static_cast<UINT>(i), nullptr, &len);
        std::vector<char> buf(len);
        D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
        if (SUCCEEDED(m_d3d12InfoQueue->GetMessage(static_cast<UINT>(i), msg, &len)))
        {
            const char* severity = "INFO";
            switch (msg->Severity)
            {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION: severity = "CORRUPTION"; break;
            case D3D12_MESSAGE_SEVERITY_ERROR:      severity = "ERROR";      break;
            case D3D12_MESSAGE_SEVERITY_WARNING:    severity = "WARNING";    break;
            case D3D12_MESSAGE_SEVERITY_INFO:       severity = "INFO";       break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:    severity = "MESSAGE";    break;
            }
            fprintf(m_logFile, "[%s] %s\n", severity, msg->pDescription);
        }
    }
    m_d3d12InfoQueue->ClearStoredMessages();
    fflush(m_logFile);
}

void TankSandboxApp::LogFps(float cpuFrameTimeMs)
{
    if (!m_logFile || cpuFrameTimeMs <= 0.0f)
    {
        return;
    }

    const UINT64 interval = m_commandLineOptions.logFpsInterval;
    if (interval == 0)
    {
        return;
    }

    m_fpsLogFrameCounter++;
    if (m_fpsLogFrameCounter % interval == 0)
    {
        fprintf(m_logFile, "[FPS] Frame %llu: %.1f FPS (%.2f ms)\n",
            static_cast<unsigned long long>(m_fpsLogFrameCounter),
            1000.0f / cpuFrameTimeMs,
            cpuFrameTimeMs);
        fflush(m_logFile);
    }
}

void TankSandboxApp::InitializeImGui()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = kImGuiDescriptorCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_graphicsDevice.Device()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_imguiHeap)));

    m_imguiSystem.Initialize(
        Win32Application::GetHwnd(),
        m_graphicsDevice,
        m_imguiHeap.Get(),
        2,
        kSwapChainFormat);
}

void TankSandboxApp::UpdateUiFrame()
{
    m_imguiSystem.BeginFrame();
    DrawToolUi();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    m_cameraPanelCtx.camera = ActiveCamera();
    m_cameraPanelCtx.trackedVehicleActive = (m_appMode == AppMode::PhysicsTrackedVehicle);
    m_cameraPanelCtx.vehicleState = &m_trackedVehicleMode.TestState();
    m_cameraPanelCtx.vehicleScene = &m_trackedVehicleMode.GetScene();

    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 950, 10), ImGuiCond_FirstUseEver);
    Ui::DrawCameraPanel(m_cameraPanelCtx);

    // RtPbrSurvey Debug at top-right of viewport
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 430, 10), ImGuiCond_FirstUseEver);
    const RtPbrSurveyEngine::LightingParams currentLighting =
        m_sceneRenderer.GetLightingParams();
    m_environmentMappingUi.lighting.lightDirection = currentLighting.lightDirection;
    m_environmentMappingUi.lighting.lightColor = currentLighting.lightColor;
    m_environmentMappingUi.lighting.diffuseIntensity = currentLighting.diffuseIntensity;
    m_environmentMappingUi.lighting.directLightEnabled = currentLighting.directLightEnabled;
    m_environmentMappingUi.lighting.emissiveEnabled = currentLighting.emissiveEnabled;
    RtPbrSurvey::SceneRendererDebugUi::Draw(
        m_sceneRenderer,
        &m_rendererDebugOpen,
        "RtPbrSurvey Debug",
        &m_environmentMappingUi);

    // Renderer Settings to the left of RtPbrSurvey Debug
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 740, 10), ImGuiCond_FirstUseEver);
    Ui::DrawRendererSettingsPanel(m_rendererPanelCtx);

    m_imguiSystem.EndFrame();
}

void TankSandboxApp::RequestScreenshot()
{
    SYSTEMTIME localTime = {};
    GetLocalTime(&localTime);

    wchar_t fileName[64] = {};
    swprintf_s(
        fileName,
        L"TankSandbox_%04u-%02u-%02u_%02u%02u%02u.png",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute,
        localTime.wSecond);

    wchar_t executablePath[MAX_PATH] = {};
    const DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (executablePathLength == 0 || executablePathLength == MAX_PATH)
    {
        m_screenshotStatus = "Capture failed: cannot resolve executable path";
        return;
    }

    const std::filesystem::path path =
        std::filesystem::path(executablePath).parent_path() / "Screenshots" / fileName;
    m_sceneRenderer.RequestScreenshot({ path });
    m_screenshotStatus = "Capture requested: " + path.string();
}

void TankSandboxApp::UpdateScreenshotResult()
{
    const std::optional<RtPbrSurvey::ScreenshotResult> result = m_sceneRenderer.ConsumeScreenshotResult();
    if (!result)
    {
        return;
    }

    if (result->succeeded)
    {
        m_screenshotStatus = "Saved: " + result->path.string();
    }
    else
    {
        m_screenshotStatus = "Capture failed: " + result->error;
    }
}

bool TankSandboxApp::SaveRendererSettings()
{
    const std::filesystem::path path(kRendererSettingsPath);
    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    if (errorCode)
    {
        m_rendererSettingsStatus = "Save failed: " + errorCode.message();
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        m_rendererSettingsStatus = "Save failed: cannot open file";
        return false;
    }

    output << RtPbrSurvey::SerializeSceneRendererSettings(m_sceneRenderer.CaptureSettings());
    if (!output)
    {
        m_rendererSettingsStatus = "Save failed: cannot write file";
        return false;
    }

    m_rendererSettingsStatus = "Saved";
    return true;
}

bool TankSandboxApp::LoadRendererSettings()
{
    std::ifstream input(kRendererSettingsPath, std::ios::binary);
    if (!input)
    {
        m_rendererSettingsStatus = "No saved settings";
        return false;
    }

    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    RtPbrSurvey::SceneRendererSettings settings = m_sceneRenderer.CaptureSettings();
    std::string error;
    if (!RtPbrSurvey::DeserializeSceneRendererSettings(json, settings, &error))
    {
        m_rendererSettingsStatus = "Load failed: " + error;
        return false;
    }

    m_sceneRenderer.ApplySettings(settings);
    m_environmentMappingUi.lighting = m_sceneRenderer.GetLightingParams();
    m_environmentMappingUi.iblEnabled =
        m_environmentMappingUi.lighting.diffuseIblEnabled ||
        m_environmentMappingUi.lighting.specularIblEnabled;
    m_rendererSettingsStatus = "Loaded";
    return true;
}

void TankSandboxApp::ResetRendererSettings()
{
    m_sceneRenderer.ApplySettings(m_defaultRendererSettings);
    m_environmentMappingUi.lighting = m_sceneRenderer.GetLightingParams();
    m_environmentMappingUi.iblEnabled =
        m_environmentMappingUi.lighting.diffuseIblEnabled ||
        m_environmentMappingUi.lighting.specularIblEnabled;
    m_rendererSettingsStatus = "Reset to Tank defaults";
}

bool TankSandboxApp::SaveCameraSettings()
{
    Engine::CameraState* camera = ActiveCamera();
    if (camera == nullptr)
    {
        return false;
    }
    const Tank::Rendering::CameraSettings settings =
        m_cameraController.CaptureSettings(*camera, m_cameraController.FollowEnabled());

    Tank::App::CameraSettingsStore store(m_cameraController.SelectedSlot());
    std::string writeStatus;
    if (!store.Write(settings, writeStatus))
    {
        return false;
    }
    m_cameraController.SetSlotSettings(
        m_cameraController.SelectedSlot(),
        settings);
    return true;
}

bool TankSandboxApp::LoadCameraSettings()
{
    Engine::CameraState* camera = ActiveCamera();
    if (camera == nullptr)
    {
        return false;
    }
    if (!m_cameraController.EnsureSlotLoaded(
        m_cameraController.SelectedSlot()))
    {
        return false;
    }
    const Tank::Rendering::CameraSettings* cached =
        m_cameraController.GetCachedSettings();
    if (cached == nullptr)
    {
        return false;
    }
    m_cameraController.ApplySettings(*cached, true, *camera);
    return true;
}

void TankSandboxApp::DrawToolUi()
{
    switch (m_appMode)
    {
    case AppMode::TopMenu:
        DrawTopMenuUi();
        break;
    case AppMode::PhysicsBoxDrop:
        m_boxDropMode.DrawUi(m_sceneRenderer, m_sceneRenderer.CpuFrameTimeMs());
        break;
    case AppMode::PhysicsTrackedVehicle:
        {
            const Tank::Input::GamepadState& gp = m_gamepad.State();
            m_trackedVehiclePanelCtx.gamepadState = gp;
            m_trackedVehiclePanelCtx.gamepadAvailable = m_gamepad.IsAvailable();
            m_trackedVehiclePanelCtx.cpuFrameTimeMs = m_sceneRenderer.CpuFrameTimeMs();
            m_trackedVehiclePanelCtx.analogLeftTrack = m_trackedVehicleMode.AnalogLeftTrack();
            m_trackedVehiclePanelCtx.analogRightTrack = m_trackedVehicleMode.AnalogRightTrack();
            m_trackedVehiclePanelCtx.analogRoll = m_trackedVehicleMode.AnalogRoll();
            m_trackedVehiclePanelCtx.analogTracksConnected = m_trackedVehicleMode.AnalogTracksConnected();
            Ui::DrawTrackedVehiclePanel(m_trackedVehiclePanelCtx);
        }
        break;
    }
}

void TankSandboxApp::DrawTopMenuUi()
{
    ImGui::Begin("Tank Sandbox");
    ImGui::Text("Physics Test Scenes");
    ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
    if (ImGui::Button("Box Drop"))
    {
        EnterBoxDropMode();
    }
    if (ImGui::Button("Tracked Vehicle"))
    {
        EnterTrackedVehicleMode();
    }
    ImGui::End();
}

void TankSandboxApp::EnterBoxDropMode()
{
    m_boxDropMode.Enter(m_sceneRenderer);
    ActivateOrbitCamera(m_boxDropMode.GetScene(), { 0.0f, 1.0f, 0.0f });
    m_appMode = AppMode::PhysicsBoxDrop;
}

void TankSandboxApp::EnterTrackedVehicleMode()
{
    m_trackedVehicleMode.Enter(m_sceneRenderer);
    ActivateOrbitCamera(m_trackedVehicleMode.GetScene(), { 0.0f, 0.8f, 0.0f });
    m_appMode = AppMode::PhysicsTrackedVehicle;
    if (m_cameraController.AutoLoad())
    {
        LoadCameraSettings();
    }
}

void TankSandboxApp::ActivateOrbitCamera(Engine::Scene& scene, const XMFLOAT3& pivot)
{
    m_debugCameraController.ResetInputState();
    m_debugCameraController.SetCameraState(&scene.camera);
    m_debugCameraController.SetWindowSize(GetWidth(), GetHeight());
    m_debugCameraController.SetMode(RtPbrSurvey::DebugCameraController::Mode::Arcball);

    const float offsetX = scene.camera.pos.x - pivot.x;
    const float offsetY = scene.camera.pos.y - pivot.y;
    const float offsetZ = scene.camera.pos.z - pivot.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
    const float safeDistance = (std::max)(distance, 0.1f);
    const float yaw = std::atan2(offsetX, offsetZ);
    const float pitch = std::asin(std::clamp(offsetY / safeDistance, -1.0f, 1.0f));
    m_debugCameraController.SetObjectViewerState(yaw, pitch, safeDistance, pivot);
}

void TankSandboxApp::ApplyActiveCameraScene()
{
    if (Engine::CameraState* camera = ActiveCamera())
    {
        m_sceneRenderer.SetCamera(*camera);
    }
}

Engine::CameraState* TankSandboxApp::ActiveCamera()
{
    switch (m_appMode)
    {
    case AppMode::PhysicsBoxDrop:
        return m_boxDropMode.ActiveCamera();
    case AppMode::PhysicsTrackedVehicle:
        return m_trackedVehicleMode.ActiveCamera();
    case AppMode::TopMenu:
        return nullptr;
    }

    return nullptr;
}
