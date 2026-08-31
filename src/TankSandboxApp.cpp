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
    constexpr const wchar_t* kTankModelAssetPath =
        L"Assets/TankModels/TankModel-2026-08-27-v001.glb";

    std::filesystem::path ResolveRuntimePath(const std::filesystem::path& relativePath)
    {
        std::array<wchar_t, 32768> executablePath = {};
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            return relativePath;
        }
        return std::filesystem::path(executablePath.data()).parent_path() /
            relativePath;
    }

    std::filesystem::path ResolveMapsDirectory()
    {
        const std::filesystem::path workingDirectoryMaps = "Config/Maps";
        std::error_code errorCode;
        if (std::filesystem::exists(workingDirectoryMaps, errorCode))
        {
            return workingDirectoryMaps;
        }

        std::array<wchar_t, 32768> executablePath = {};
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            return workingDirectoryMaps;
        }
        return std::filesystem::path(executablePath.data()).parent_path() /
            "Config/Maps";
    }

    bool LoadMapDocument(
        const std::filesystem::path& path,
        Tank::Physics::MapDocument& document,
        std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "cannot open " + path.string();
            return false;
        }
        const std::istreambuf_iterator<char> begin(input);
        const std::istreambuf_iterator<char> end;
        const std::string json(begin, end);
        if (!Tank::Physics::DeserializeMapDocument(json, document, &error))
        {
            error = path.filename().string() + ": " + error;
            return false;
        }
        return true;
    }
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
        else if (arg == L"--map" && i + 1 < argc)
        {
            m_autoMapPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == L"--quit-after-capture")
        {
            m_quitAfterCapture = true;
        }
        else if (arg == L"--physics-debug-overlay")
        {
            m_trackedVehicleMode.SetPhysicsDebugOverlayDefault(true);
        }
        else if (arg == L"--benchmark-frames" && i + 1 < argc)
        {
            m_benchmarkMeasureFrames = _wtoi64(argv[++i]);
        }
        else if (arg == L"--benchmark-output" && i + 1 < argc)
        {
            m_benchmarkOutputPath = argv[++i];
        }
        else if (arg == L"--benchmark-track-shoes-off")
        {
            m_benchmarkTrackShoesOff = true;
        }
        else if (arg == L"--benchmark-track-shoes-on")
        {
            m_benchmarkTrackShoesOn = true;
        }
        else if (arg == L"--benchmark-shadows-off")
        {
            m_benchmarkShadowsOff = true;
        }
        else if (arg == L"--benchmark-shadows-on")
        {
            m_benchmarkShadowsOn = true;
        }
        else if (arg == L"--benchmark-reflections-off")
        {
            m_benchmarkReflectionsOff = true;
        }
        else if (arg == L"--benchmark-reflections-on")
        {
            m_benchmarkReflectionsOn = true;
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
    RtPbrSurvey::SceneRendererHostDesc rendererHostDesc = {};
    rendererHostDesc.applicationName = L"TankPhysicsSandbox";
    rendererHostDesc.engineVersion = "1.0.0";
    RtPbrSurvey::SceneRenderer::ConfigureGraphicsDevice(deviceDesc, rendererHostDesc);
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
    m_trackedVehicleMode.LoadTankModelAsset(
        ResolveRuntimePath(kTankModelAssetPath));

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
    ReloadCustomMaps();
    LoadRendererSettings();
    if (m_benchmarkTrackShoesOff || m_benchmarkTrackShoesOn)
    {
        m_trackedVehicleMode.TrackShoeDisplay() = m_benchmarkTrackShoesOn;
    }
    if (m_benchmarkShadowsOff || m_benchmarkShadowsOn)
    {
        auto benchmarkShadows = m_sceneRenderer.GetShadowSettings();
        benchmarkShadows.enabled = m_benchmarkShadowsOn;
        m_sceneRenderer.SetShadowSettings(benchmarkShadows);
    }
    if (m_benchmarkReflectionsOff || m_benchmarkReflectionsOn)
    {
        auto benchmarkReflections = m_sceneRenderer.GetHybridReflectionSettings();
        benchmarkReflections.enabled = m_benchmarkReflectionsOn;
        m_sceneRenderer.SetHybridReflectionSettings(benchmarkReflections);
    }
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
    m_trackedVehiclePanelCtx.activeMapName = &m_trackedVehicleMode.ActiveMapName();
    m_trackedVehiclePanelCtx.physicsDebugOverlay = &m_trackedVehicleMode.PhysicsDebugOverlay();
    m_trackedVehiclePanelCtx.trackShoeDisplay = &m_trackedVehicleMode.TrackShoeDisplay();
    m_trackedVehiclePanelCtx.showTrackProxies = &m_trackedVehicleMode.ShowTrackProxies();
    m_trackedVehiclePanelCtx.showDummyModel = &m_trackedVehicleMode.ShowDummyModel();
    m_trackedVehiclePanelCtx.showDummyWheels = &m_trackedVehicleMode.ShowDummyWheels();
    m_trackedVehiclePanelCtx.showGltfBody = &m_trackedVehicleMode.ShowGltfBody();
    m_trackedVehiclePanelCtx.showGltfCannon = &m_trackedVehicleMode.ShowGltfCannon();
    m_trackedVehiclePanelCtx.showGltfSide = &m_trackedVehicleMode.ShowGltfSide();
    m_trackedVehiclePanelCtx.tankModelLoadStatus =
        &m_trackedVehicleMode.TankModelLoadStatus();
    m_trackedVehiclePanelCtx.trackedVehiclePaused = &m_trackedVehicleMode.Paused();
    m_trackedVehiclePanelCtx.trackedVehicleSingleStep = &m_trackedVehicleMode.SingleStep();
    m_trackedVehiclePanelCtx.tankSettingsSlot = &m_trackedVehicleMode.TankSettingsSlot();
    m_trackedVehiclePanelCtx.tankSettingsAutoLoad = &m_trackedVehicleMode.TankSettingsAutoLoad();
    m_trackedVehiclePanelCtx.tankVisualSettingsAutoLoad = &m_trackedVehicleMode.TankVisualSettingsAutoLoad();
    m_trackedVehiclePanelCtx.tankVisualMaterialApplyPending = &m_trackedVehicleMode.TankVisualMaterialApplyPending();
    m_trackedVehiclePanelCtx.tankSettingsStatus = &m_trackedVehicleMode.TankSettingsStatus();
    m_trackedVehiclePanelCtx.tankVisualSettingsStatus = &m_trackedVehicleMode.TankVisualSettingsStatus();
    m_trackedVehiclePanelCtx.envSettingsStatus = &m_trackedVehicleMode.EnvSettingsStatus();
    m_trackedVehiclePanelCtx.tankModelExportPath =
        &m_trackedVehicleMode.TankModelExportPath();
    m_trackedVehiclePanelCtx.tankModelExportStatus =
        &m_trackedVehicleMode.TankModelExportStatus();
    m_trackedVehiclePanelCtx.tankModelExportBinary =
        &m_trackedVehicleMode.TankModelExportBinary();
    m_trackedVehiclePanelCtx.tankSettings = &m_trackedVehicleMode.Settings();
    m_trackedVehiclePanelCtx.inputMappingSettings = &m_trackedVehicleMode.InputMappingSettings();
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
    m_trackedVehiclePanelCtx.fireRecoil = [this]()
    {
        m_trackedVehicleMode.FireRecoil();
    };
    m_trackedVehiclePanelCtx.applyMaterials = [this]()
    {
        m_trackedVehicleMode.ApplyMaterials(m_sceneRenderer);
    };
    m_trackedVehiclePanelCtx.saveTankSettings = [this]()
    {
        m_trackedVehicleMode.SaveTankSettings();
    };
    m_trackedVehiclePanelCtx.saveInputMappingSettings = [this]()
    {
        m_trackedVehicleMode.SaveInputMappingSettings();
    };
    m_trackedVehiclePanelCtx.loadInputMappingSettings = [this]()
    {
        m_trackedVehicleMode.LoadInputMappingSettings();
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
    m_trackedVehiclePanelCtx.exportTankModel = [this]()
    {
        m_trackedVehicleMode.ExportTankModel();
    };
    m_trackedVehiclePanelCtx.resetFrameTimingPeaks = [this]()
    {
        m_peakCpuFrameTimeMs = m_sceneRenderer.CpuFrameTimeMs();
        m_cpuFrameTimeSamples.fill(0.0f);
        m_cpuFrameTimeSampleIndex = 0;
        m_cpuFrameTimeSamplesRecorded = 0;
        m_averageCpuFrameTimeMs = 0.0f;
        m_p95CpuFrameTimeMs = 0.0f;
        m_p99CpuFrameTimeMs = 0.0f;
        m_trackedVehicleMode.ResetFrameTimingPeaks();
    };

    if (m_autoSceneMode.has_value())
    {
        if (*m_autoSceneMode == AppMode::PhysicsTrackedVehicle &&
            m_autoMapPath && !LoadAutoMap())
        {
            m_autoSceneMode.reset();
            return;
        }
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

bool TankSandboxApp::EnsureDebugCameraForMouse()
{
    if (m_appMode == AppMode::TopMenu || ImGui::GetIO().WantCaptureMouse)
    {
        return false;
    }
    const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    Engine::CameraState* camera = ActiveCamera();
    if (camera == nullptr)
    {
        return false;
    }
    if (m_cameraController.IsDebugSlot())
    {
        return true;
    }

    const Tank::App::CameraController::MouseControlMode mouseMode =
        m_cameraController.GetMouseControlMode();
    if (mouseMode == Tank::App::CameraController::MouseControlMode::Gameplay ||
        (mouseMode == Tank::App::CameraController::MouseControlMode::AltGesture &&
            !altDown))
    {
        return false;
    }

    Engine::Scene* scene = ActiveScene();
    if (scene == nullptr)
    {
        return false;
    }

    const bool preservePositionFollow = m_cameraController.FollowEnabled() &&
        m_appMode == AppMode::PhysicsTrackedVehicle;
    m_cameraController.CancelTransition();
    if (preservePositionFollow)
    {
        m_cameraController.AdoptCurrentFollowPose(
            m_trackedVehicleMode.TestState(), *camera);
    }
    const Tank::Rendering::CameraSettings debugSettings =
        m_cameraController.CaptureSettings(*camera, preservePositionFollow);
    m_cameraController.SetSlotSettings(
        Tank::App::CameraController::kDebugSlot,
        debugSettings);
    m_cameraController.SelectSlot(
        Tank::App::CameraController::kDebugSlot,
        false);
    m_cameraController.SetTankYawChaseEnabled(!preservePositionFollow);
    m_cameraController.SetFollowEnabled(preservePositionFollow);
    ActivateOrbitCamera(*scene, camera->gazePoint);
    return true;
}

bool TankSandboxApp::HasInputFocus() const
{
    const HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == nullptr)
    {
        return false;
    }

    DWORD foregroundProcessId = 0;
    GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
    return foregroundProcessId == GetCurrentProcessId();
}

void TankSandboxApp::ClearVehicleInputState()
{
    m_moveForward = false;
    m_moveBackward = false;
    m_turnLeft = false;
    m_turnRight = false;
    m_pivotTurnModifier = false;
    m_rollLeft = false;
    m_rollRight = false;
    m_brake = false;
}

void TankSandboxApp::OnMouseDown(UINT8 button, int x, int y)
{
    if (EnsureDebugCameraForMouse())
    {
        m_debugCameraController.OnMouseDown(button, x, y);
    }
}

void TankSandboxApp::OnMouseUp(UINT8 button, int x, int y)
{
    if (EnsureDebugCameraForMouse())
    {
        m_debugCameraController.OnMouseUp(button, x, y);
        ApplyActiveCameraScene();
    }
}

void TankSandboxApp::OnMouseMove(int x, int y)
{
    if (EnsureDebugCameraForMouse())
    {
        m_debugCameraController.OnMouseMove(x, y);
        ApplyActiveCameraScene();
    }
}

void TankSandboxApp::OnMouseWheel(int wheelDelta)
{
    if (EnsureDebugCameraForMouse())
    {
        Engine::CameraState* camera = ActiveCamera();
        if (camera != nullptr &&
            camera->projection == Engine::CameraProjection::Orthographic)
        {
            const float steps = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
            camera->orthographicHeight = std::clamp(
                camera->orthographicHeight - steps, 1.0f, 200.0f);
            ApplyActiveCameraScene();
        }
        else
        {
            m_debugCameraController.OnMouseWheel(wheelDelta, false);
            ApplyActiveCameraScene();
        }
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
    const bool hasInputFocus = HasInputFocus();
    if (!hasInputFocus)
    {
        ClearVehicleInputState();
    }

    if (m_appMode == AppMode::PhysicsBoxDrop)
    {
        m_boxDropMode.Update(m_sceneRenderer);
    }
    else if (m_appMode == AppMode::PhysicsTrackedVehicle)
    {
        m_gamepad.Poll();
        const Tank::Input::GamepadState neutralGamepadState;
        const Tank::Input::GamepadState& vehicleGamepadState =
            hasInputFocus ? m_gamepad.State() : neutralGamepadState;
        {
            const Tank::Input::GamepadState& gp = vehicleGamepadState;
            if (m_cameraController.UpdateButtonStates(
                    gp.connected && gp.buttonCount > 4 && gp.rawButtons[4],
                    gp.connected && gp.buttonCount > 7 && gp.rawButtons[7]))
            {
                LoadCameraSettings();
            }
            const float orbitHorizontal =
                (gp.dpadRight ? 1.0f : 0.0f) - (gp.dpadLeft ? 1.0f : 0.0f);
            const float orbitVertical =
                (gp.dpadUp ? 1.0f : 0.0f) - (gp.dpadDown ? 1.0f : 0.0f);
            m_cameraController.UpdateChaseOrbitInput(
                orbitHorizontal,
                orbitVertical,
                TrackedVehicleMode::kPhysicsFixedDt);
        }
        m_trackedVehicleMode.UpdateInput(
            vehicleGamepadState,
            m_moveForward, m_moveBackward,
            m_turnLeft, m_turnRight, m_pivotTurnModifier,
            m_rollLeft, m_rollRight, m_brake);
        m_trackedVehicleMode.Step(m_sceneRenderer, m_cameraController);
        if (m_cameraController.IsDebugSlot() &&
            m_cameraController.FollowEnabled() &&
            !m_cameraController.TankYawChaseEnabled())
        {
            if (Engine::CameraState* camera = ActiveCamera())
            {
                m_debugCameraController.SetObjectViewerState(
                    m_debugCameraController.ObjectViewerYaw(),
                    m_debugCameraController.ObjectViewerPitch(),
                    m_debugCameraController.ObjectViewerDistance(),
                    camera->gazePoint);
                ApplyActiveCameraScene();
            }
        }

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
    m_peakCpuFrameTimeMs = (std::max)(
        m_peakCpuFrameTimeMs,
        m_sceneRenderer.CpuFrameTimeMs());
    const float cpuFrameTimeMs = m_sceneRenderer.CpuFrameTimeMs();
    if (cpuFrameTimeMs > 0.0f)
    {
        m_cpuFrameTimeSamples[m_cpuFrameTimeSampleIndex] = cpuFrameTimeMs;
        m_cpuFrameTimeSampleIndex =
            (m_cpuFrameTimeSampleIndex + 1) % kFrameTimingSampleCount;
        m_cpuFrameTimeSamplesRecorded = (std::min)(
            m_cpuFrameTimeSamplesRecorded + 1,
            kFrameTimingSampleCount);
        if (m_cpuFrameTimeSamplesRecorded == kFrameTimingSampleCount &&
            m_cpuFrameTimeSampleIndex % 30 == 0)
        {
            std::array<float, kFrameTimingSampleCount> sorted =
                m_cpuFrameTimeSamples;
            std::sort(sorted.begin(), sorted.end());
            float sum = 0.0f;
            for (float sample : sorted)
            {
                sum += sample;
            }
            m_averageCpuFrameTimeMs =
                sum / static_cast<float>(kFrameTimingSampleCount);
            m_p95CpuFrameTimeMs = sorted[284];
            m_p99CpuFrameTimeMs = sorted[296];
        }
    }
    if (m_benchmarkMeasureFrames > 0)
    {
        ++m_benchmarkElapsedFrames;
        if (m_benchmarkElapsedFrames > m_benchmarkWarmupFrames)
        {
            m_benchmarkCpuFrameTimes.push_back(cpuFrameTimeMs);
            if (m_benchmarkCpuFrameTimes.size() >= m_benchmarkMeasureFrames)
            {
                FinishFrameBenchmark();
                PostQuitMessage(0);
            }
        }
    }
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

void TankSandboxApp::FinishFrameBenchmark()
{
    if (m_benchmarkCpuFrameTimes.empty() || m_benchmarkOutputPath.empty())
    {
        return;
    }

    std::vector<float> sorted = m_benchmarkCpuFrameTimes;
    std::sort(sorted.begin(), sorted.end());
    float sum = 0.0f;
    for (float sample : sorted)
    {
        sum += sample;
    }
    const auto percentile = [&sorted](float fraction)
    {
        const size_t index = (std::min)(
            static_cast<size_t>(fraction * static_cast<float>(sorted.size() - 1)),
            sorted.size() - 1);
        return sorted[index];
    };

    std::error_code errorCode;
    if (!m_benchmarkOutputPath.parent_path().empty())
    {
        std::filesystem::create_directories(
            m_benchmarkOutputPath.parent_path(),
            errorCode);
    }
    std::ofstream output(m_benchmarkOutputPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return;
    }
    output << "{\r\n"
           << "  \"frames\": " << sorted.size() << ",\r\n"
           << "  \"trackShoesEnabled\": "
           << (m_trackedVehicleMode.TrackShoeDisplay() ? "true" : "false") << ",\r\n"
           << "  \"shadowsEnabled\": "
           << (m_sceneRenderer.GetShadowSettings().enabled ? "true" : "false") << ",\r\n"
           << "  \"reflectionsEnabled\": "
           << (m_sceneRenderer.GetHybridReflectionSettings().enabled ? "true" : "false") << ",\r\n"
           << "  \"averageCpuFrameTimeMs\": " << sum / static_cast<float>(sorted.size()) << ",\r\n"
           << "  \"p95CpuFrameTimeMs\": " << percentile(0.95f) << ",\r\n"
           << "  \"p99CpuFrameTimeMs\": " << percentile(0.99f) << ",\r\n"
           << "  \"maximumCpuFrameTimeMs\": " << sorted.back() << ",\r\n"
           << "  \"physicsPeakTimeMs\": " << m_trackedVehicleMode.PhysicsStepPeakTimeMs() << ",\r\n"
           << "  \"sceneUpdatePeakTimeMs\": " << m_trackedVehicleMode.SceneUpdatePeakTimeMs() << "\r\n"
           << "}\r\n";
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
        m_rendererSettingsStatus =
            "No saved settings: " +
            std::filesystem::absolute(kRendererSettingsPath).string();
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

    if (m_cameraController.IsDebugSlot() && m_appMode == AppMode::PhysicsTrackedVehicle)
    {
        const Tank::Physics::TrackedVehicleTestState& state = m_trackedVehicleMode.TestState();
        ActivateOrbitCamera(m_trackedVehicleMode.GetScene(),
            { state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
        return true;
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
    m_cameraController.SetTankYawChaseEnabled(
        !m_cameraController.IsDebugSlot());
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
            m_trackedVehiclePanelCtx.peakCpuFrameTimeMs = m_peakCpuFrameTimeMs;
            m_trackedVehiclePanelCtx.averageCpuFrameTimeMs = m_averageCpuFrameTimeMs;
            m_trackedVehiclePanelCtx.p95CpuFrameTimeMs = m_p95CpuFrameTimeMs;
            m_trackedVehiclePanelCtx.p99CpuFrameTimeMs = m_p99CpuFrameTimeMs;
            m_trackedVehiclePanelCtx.physicsStepTimeMs =
                m_trackedVehicleMode.PhysicsStepTimeMs();
            m_trackedVehiclePanelCtx.physicsStepPeakTimeMs =
                m_trackedVehicleMode.PhysicsStepPeakTimeMs();
            m_trackedVehiclePanelCtx.sceneUpdateTimeMs =
                m_trackedVehicleMode.SceneUpdateTimeMs();
            m_trackedVehiclePanelCtx.sceneUpdatePeakTimeMs =
                m_trackedVehicleMode.SceneUpdatePeakTimeMs();
            m_trackedVehiclePanelCtx.analogLeftTrack = m_trackedVehicleMode.AnalogLeftTrack();
            m_trackedVehiclePanelCtx.analogRightTrack = m_trackedVehicleMode.AnalogRightTrack();
            m_trackedVehiclePanelCtx.analogRoll = m_trackedVehicleMode.AnalogRoll();
            m_trackedVehiclePanelCtx.analogTracksConnected = m_trackedVehicleMode.AnalogTracksConnected();
            m_trackedVehiclePanelCtx.analogTracksArmed = m_trackedVehicleMode.AnalogTracksArmed();
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
    ImGui::SeparatorText("Tracked Vehicle Map");
    for (const Tank::Physics::MapDefinition& map :
         Tank::Physics::GetMapDefinitions())
    {
        const bool selected = !m_selectedCustomMap && m_selectedMap == map.id;
        if (ImGui::RadioButton(map.name, selected))
        {
            m_selectedMap = map.id;
            m_selectedCustomMap.reset();
        }
    }
    for (size_t index = 0; index < m_customMaps.size(); ++index)
    {
        ImGui::PushID(static_cast<int>(index));
        const bool selected = m_selectedCustomMap == index;
        if (ImGui::RadioButton(m_customMaps[index].document.name.c_str(), selected))
        {
            m_selectedCustomMap = index;
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Reload Map Files"))
    {
        ReloadCustomMaps();
    }
    if (!m_customMapStatus.empty())
    {
        ImGui::TextUnformatted(m_customMapStatus.c_str());
    }
    if (ImGui::Button("Start Tracked Vehicle"))
    {
        if (m_selectedCustomMap && *m_selectedCustomMap < m_customMaps.size())
        {
            m_trackedVehicleMode.SelectCustomMap(
                m_customMaps[*m_selectedCustomMap].document);
        }
        else
        {
            m_trackedVehicleMode.SelectMap(m_selectedMap);
        }
        EnterTrackedVehicleMode();
    }
    ImGui::End();
}

void TankSandboxApp::ReloadCustomMaps()
{
    m_customMaps.clear();
    m_selectedCustomMap.reset();
    const std::filesystem::path mapsDirectory = ResolveMapsDirectory();
    std::error_code errorCode;
    if (!std::filesystem::exists(mapsDirectory, errorCode))
    {
        m_customMapStatus = "No Config/Maps directory";
        return;
    }

    size_t rejectedCount = 0;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(mapsDirectory, errorCode))
    {
        if (errorCode || !entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }
        Tank::Physics::MapDocument document;
        std::string error;
        if (!LoadMapDocument(entry.path(), document, error))
        {
            ++rejectedCount;
            continue;
        }
        m_customMaps.push_back({ entry.path().filename().string(), std::move(document) });
    }
    std::sort(
        m_customMaps.begin(),
        m_customMaps.end(),
        [](const CustomMapEntry& left, const CustomMapEntry& right)
        {
            return left.fileName < right.fileName;
        });
    m_customMapStatus = std::to_string(m_customMaps.size()) + " map file(s) loaded";
    if (rejectedCount > 0)
    {
        m_customMapStatus += ", " + std::to_string(rejectedCount) + " rejected";
    }
}

bool TankSandboxApp::LoadAutoMap()
{
    if (!m_autoMapPath)
    {
        return true;
    }
    Tank::Physics::MapDocument document;
    std::string error;
    if (!LoadMapDocument(*m_autoMapPath, document, error))
    {
        m_customMapStatus = "Map load failed: " + error;
        return false;
    }
    m_trackedVehicleMode.SelectCustomMap(document);
    m_customMapStatus = "Auto map: " + document.name;
    return true;
}

void TankSandboxApp::EnterBoxDropMode()
{
    m_boxDropMode.Enter(m_sceneRenderer);
    ActivateOrbitCamera(m_boxDropMode.GetScene(), { 0.0f, 1.0f, 0.0f });
    m_appMode = AppMode::PhysicsBoxDrop;
}

void TankSandboxApp::EnterTrackedVehicleMode()
{
    ClearVehicleInputState();
    m_peakCpuFrameTimeMs = 0.0f;
    m_cpuFrameTimeSamples.fill(0.0f);
    m_cpuFrameTimeSampleIndex = 0;
    m_cpuFrameTimeSamplesRecorded = 0;
    m_averageCpuFrameTimeMs = 0.0f;
    m_p95CpuFrameTimeMs = 0.0f;
    m_p99CpuFrameTimeMs = 0.0f;
    m_trackedVehicleMode.ResetFrameTimingPeaks();
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
        if (m_cameraController.IsDebugSlot())
        {
            Tank::App::CameraController::StabilizeWorldUp(*camera);
        }
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

Engine::Scene* TankSandboxApp::ActiveScene()
{
    switch (m_appMode)
    {
    case AppMode::PhysicsBoxDrop:
        return &m_boxDropMode.GetScene();
    case AppMode::PhysicsTrackedVehicle:
        return &m_trackedVehicleMode.GetScene();
    case AppMode::TopMenu:
        return nullptr;
    }

    return nullptr;
}
