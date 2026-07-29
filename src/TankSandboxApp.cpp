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

namespace
{
	constexpr float kAnalogTrackDeadzone = 0.1f;

	float NormalizeRawGamepadAxis(float value)
	{
		const float normalized = std::clamp((value - 0.5f) * 2.0f, -1.0f, 1.0f);
		if (std::abs(normalized) <= kAnalogTrackDeadzone)
		{
			return 0.0f;
		}

		const float magnitude =
			(std::abs(normalized) - kAnalogTrackDeadzone) / (1.0f - kAnalogTrackDeadzone);
		return std::copysign(std::min(magnitude, 1.0f), normalized);
	}
}

#include "Physics/BoxDropTest.h"
#include "Physics/PhysicsEnvironmentSettingsJson.h"
#include "Physics/TankSettingsJson.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Input/GamepadState.h"

using namespace DirectX;

namespace
{
	constexpr const char* kRendererSettingsPath = "Config/renderer_debug.json";
	constexpr const char* kTankSettingsPath = "Config/tank_physics.json";
	constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";

	std::vector<uint8_t> CreateGroundGridTexture(uint32_t size)
	{
		constexpr uint8_t groundR = 98;
		constexpr uint8_t groundG = 91;
		constexpr uint8_t groundB = 72;
		constexpr uint8_t lineR = 165;
		constexpr uint8_t lineG = 158;
		constexpr uint8_t lineB = 132;
		constexpr uint32_t lineWidth = 2;

		std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
		for (uint32_t y = 0; y < size; ++y)
		{
			for (uint32_t x = 0; x < size; ++x)
			{
				const bool line =
					x < lineWidth || y < lineWidth ||
					x >= size - lineWidth || y >= size - lineWidth;
				const size_t pixel = (static_cast<size_t>(y) * size + x) * 4;
				pixels[pixel + 0] = line ? lineR : groundR;
				pixels[pixel + 1] = line ? lineG : groundG;
				pixels[pixel + 2] = line ? lineB : groundB;
				pixels[pixel + 3] = 255;
			}
		}
		return pixels;
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
		else if (arg == L"--quit-after-capture")
		{
			m_quitAfterCapture = true;
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
	m_gamepad.Initialize();

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
			m_boxDropSceneBuilder.Clear();
			m_trackedVehicleSceneBuilder.Clear();
			m_sceneRenderer.SetScene(Engine::Scene{});
		}
		else
		{
			PostQuitMessage(0);
		}
	}
	else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'R')
	{
		ResetTrackedVehicle();
	}
	else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'P')
	{
		m_trackedVehiclePaused = !m_trackedVehiclePaused;
	}
	else if (m_appMode == AppMode::PhysicsTrackedVehicle && key == 'N' && m_trackedVehiclePaused)
	{
		m_trackedVehicleSingleStep = true;
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
	if (m_appMode != AppMode::TopMenu)
	{
		m_debugCameraController.OnMouseDown(button, x, y);
	}
}

void TankSandboxApp::OnMouseUp(UINT8 button, int x, int y)
{
	if (m_appMode != AppMode::TopMenu)
	{
		m_debugCameraController.OnMouseUp(button, x, y);
		ApplyActiveCameraScene();
	}
}

void TankSandboxApp::OnMouseMove(int x, int y)
{
	if (m_appMode != AppMode::TopMenu)
	{
		m_debugCameraController.OnMouseMove(x, y);
		ApplyActiveCameraScene();
	}
}

void TankSandboxApp::OnMouseWheel(int wheelDelta)
{
	if (m_appMode != AppMode::TopMenu)
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
		Tank::Physics::BoxDropState state = m_boxDropTest.Step(kPhysicsFixedDt);
		UpdateBoxDropScene(state);
	}
	else if (m_appMode == AppMode::PhysicsTrackedVehicle)
	{
		m_gamepad.Poll();
		UpdateTrackedVehicleInput();
		if (!m_trackedVehiclePaused || m_trackedVehicleSingleStep)
		{
			const Tank::Physics::TrackedVehicleTestState state = m_trackedVehicleTest.Step(kPhysicsFixedDt);
			UpdateTrackedVehicleScene(state);
			m_trackedVehicleSingleStep = false;
		}
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

	ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 950, 10), ImGuiCond_FirstUseEver);
	DrawCameraUi();

	// RtPbrSurvey Debug at top-right of viewport
	ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 430, 10), ImGuiCond_FirstUseEver);
	RtPbrSurvey::SceneRendererDebugUi::Draw(m_sceneRenderer, &m_rendererDebugOpen);

	// Renderer Settings to the left of RtPbrSurvey Debug
	ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 740, 10), ImGuiCond_FirstUseEver);
	DrawRendererSettingsUi();

	m_imguiSystem.EndFrame();
}

void TankSandboxApp::DrawCameraUi()
{
	Engine::CameraState* camera = ActiveCamera();
	if (camera == nullptr)
	{
		return;
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 190.0f), ImVec2(1000.0f, 1000.0f));
	ImGui::Begin("Camera");
	if (m_appMode == AppMode::PhysicsTrackedVehicle)
	{
		ImGui::SeparatorText("Angle");
		if (ImGui::Button("Rear High"))
		{
			ApplyTrackedVehicleCameraPreset({ 0.0f, 9.2f, -16.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("Rear Quarter"))
		{
			ApplyTrackedVehicleCameraPreset({ 10.0f, 7.0f, -14.0f });
		}
		if (ImGui::Button("Side High"))
		{
			ApplyTrackedVehicleCameraPreset({ 16.0f, 6.0f, 0.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("Top Rear"))
		{
			ApplyTrackedVehicleCameraPreset({ 0.0f, 18.0f, -4.0f });
		}
		ImGui::SeparatorText("Projection");
	}
	int projection = static_cast<int>(camera->projection);
	bool changed = false;
	changed |= ImGui::RadioButton(
		"Perspective", &projection, static_cast<int>(Engine::CameraProjection::Perspective));
	ImGui::SameLine();
	changed |= ImGui::RadioButton(
		"Orthographic", &projection, static_cast<int>(Engine::CameraProjection::Orthographic));
	camera->projection = static_cast<Engine::CameraProjection>(projection);

	if (camera->projection == Engine::CameraProjection::Perspective)
	{
		changed |= ImGui::SliderFloat("FOV Y", &camera->fov, 20.0f, 120.0f, "%.1f deg");
	}
	else
	{
		changed |= ImGui::SliderFloat(
			"Ortho Height", &camera->orthographicHeight, 1.0f, 50.0f, "%.1f");
	}

	if (changed)
	{
		m_sceneRenderer.SetCamera(*camera);
	}
	ImGui::End();
}

void TankSandboxApp::DrawRendererSettingsUi()
{
	ImGui::Begin("Renderer Settings");
	ImGui::TextUnformatted(kRendererSettingsPath);
	if (ImGui::Button("Save"))
	{
		SaveRendererSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		LoadRendererSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		ResetRendererSettings();
	}
	if (!m_rendererSettingsStatus.empty())
	{
		ImGui::TextWrapped("%s", m_rendererSettingsStatus.c_str());
	}
	ImGui::Separator();
	if (ImGui::Button("Capture"))
	{
		RequestScreenshot();
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("F12");
	if (!m_screenshotStatus.empty())
	{
		ImGui::TextWrapped("%s", m_screenshotStatus.c_str());
	}
	ImGui::End();
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
	m_rendererSettingsStatus = "Loaded";
	return true;
}

void TankSandboxApp::ResetRendererSettings()
{
	m_sceneRenderer.ApplySettings(m_defaultRendererSettings);
	m_rendererSettingsStatus = "Reset to Tank defaults";
}

void TankSandboxApp::DrawToolUi()
{
	switch (m_appMode)
	{
	case AppMode::TopMenu:
		DrawTopMenuUi();
		break;
	case AppMode::PhysicsBoxDrop:
		DrawPhysicsBoxDropUi();
		break;
	case AppMode::PhysicsTrackedVehicle:
		DrawPhysicsTrackedVehicleUi();
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
	m_boxDropSceneBuilder.Clear();

	uint32_t matFloor = m_boxDropSceneBuilder.AddSolidColorMaterial(128, 128, 128, 255);
	uint32_t matBox = m_boxDropSceneBuilder.AddSolidColorMaterial(200, 50, 50, 255);

	m_boxDropSceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
	m_boxDropSceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);

	m_boxDropSceneBuilder.AddInstance(
		XMMatrixScaling(20.0f, 0.2f, 20.0f) * XMMatrixTranslation(0.0f, -0.1f, 0.0f),
		matFloor);

	m_boxDropBoxInstanceIndex = 1;
	m_boxDropSceneBuilder.AddInstance(
		XMMatrixTranslation(0.0f, 5.0f, 0.0f),
		matBox);

	Engine::CameraState camera;
	camera.pos = { 0.0f, 4.0f, -12.0f };
	camera.gazePoint = { 0.0f, 1.0f, 0.0f };
	camera.fov = 60.0f;
	camera.nearZ = 0.1f;
	camera.farZ = 10000.0f;
	m_boxDropSceneBuilder.SetCamera(camera);
	ActivateOrbitCamera(m_boxDropSceneBuilder.GetScene(), { 0.0f, 1.0f, 0.0f });

	m_boxDropTest.Initialize();

	m_sceneRenderer.SetScene(m_boxDropSceneBuilder.GetScene());
	m_sceneRenderer.ReloadSceneResources(m_boxDropSceneBuilder.GetScene());
	m_sceneRenderer.SetDisplayInstanceCount(static_cast<int>(m_boxDropSceneBuilder.GetScene().instances.size()));

	m_appMode = AppMode::PhysicsBoxDrop;
}

void TankSandboxApp::UpdateBoxDropScene(const Tank::Physics::BoxDropState& state)
{
	Engine::Scene& scene = m_boxDropSceneBuilder.GetScene();
	scene.instances[m_boxDropBoxInstanceIndex].prevWorld = scene.instances[m_boxDropBoxInstanceIndex].world;
	XMMATRIX boxWorld = XMMatrixTranslation(
		state.boxPosition.x,
		state.boxPosition.y,
		state.boxPosition.z);
	XMStoreFloat4x4(&scene.instances[m_boxDropBoxInstanceIndex].world, XMMatrixTranspose(boxWorld));

	m_sceneRenderer.SetScene(scene);
}

void TankSandboxApp::DrawPhysicsBoxDropUi()
{
	const Tank::Physics::BoxDropState& state = m_boxDropTest.State();
	ImGui::Begin("Box Drop");
	ImGui::Text("Step: %d", state.stepIndex);
	ImGui::Text("Time: %.2f s", state.timeSeconds);
	ImGui::Text("Box Y: %.3f", state.boxPosition.y);
	ImGui::Text("Sleeping: %s", state.boxSleeping ? "yes" : "no");
	ImGui::Separator();
	ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
	if (ImGui::Button("Reset"))
	{
		m_boxDropTest.Initialize();
		UpdateBoxDropScene(m_boxDropTest.State());
	}
	ImGui::Separator();
	ImGui::Text("Press ESC to return to the top menu.");
	ImGui::End();
}

void TankSandboxApp::DrawPhysicsTrackedVehicleUi()
{
	const Tank::Physics::TrackedVehicleTestState& state = m_trackedVehicleTest.State();
	ImGui::SetNextWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Tracked Vehicle");
	ImGui::Text("Step: %d", state.stepIndex);
	ImGui::Text("Time: %.2f s", state.timeSeconds);
	ImGui::Text("Position: %.2f, %.2f, %.2f",
		state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
	int wheelContactCount = 0;
	for (int i = 0; i < state.wheelCount; ++i)
	{
		wheelContactCount += state.wheels[static_cast<size_t>(i)].hasContact ? 1 : 0;
	}
	ImGui::Text("Wheel contacts: %d / %d", wheelContactCount, state.wheelCount);
	ImGui::Text("Sleeping: %s", state.sleeping ? "yes" : "no");
	ImGui::Text("Controls: W/S drive, A/D skid turn, Shift+A/D pivot");
	ImGui::Text("Q/E roll, Space brake");
	const Tank::Input::GamepadState& gamepadState = m_gamepad.State();
	ImGui::SeparatorText("Gamepad");
	if (!m_gamepad.IsAvailable())
	{
		ImGui::TextUnformatted("Gamepad: GameInput unavailable");
	}
	else if (!gamepadState.connected)
	{
		ImGui::TextUnformatted("Gamepad: Not connected");
	}
	else
	{
		ImGui::TextUnformatted("Gamepad: Connected");
		ImGui::Text("Device: %s",
			gamepadState.deviceName.empty()
			? "Controller (name unavailable; identify by VID/PID)"
			: gamepadState.deviceName.c_str());
		ImGui::Text("VID: %04X  PID: %04X", gamepadState.vendorId, gamepadState.productId);
		ImGui::Text("Buttons: %u  Axes: %u  Switches: %u",
			gamepadState.buttonCount, gamepadState.axisCount, gamepadState.switchCount);
		ImGui::Text("Gamepad mapping: %s", gamepadState.hasGamepadMapping ? "yes" : "no");
		if (!gamepadState.hasGamepadMapping)
		{
			ImGui::TextUnformatted("Raw fallback: axes 0/1");
			for (std::uint32_t axis = 0;
				axis < gamepadState.axisCount && axis < gamepadState.rawAxes.size();
				++axis)
			{
				ImGui::Text("Axis %u: %.3f", axis, gamepadState.rawAxes[axis]);
			}
			ImGui::TextUnformatted("Pressed raw buttons:");
			ImGui::SameLine();
			bool anyButtonPressed = false;
			for (std::uint32_t button = 0;
				button < gamepadState.buttonCount && button < gamepadState.rawButtons.size();
				++button)
			{
				if (!gamepadState.rawButtons[button])
				{
					continue;
				}
				ImGui::SameLine();
				ImGui::Text("%u", button);
				anyButtonPressed = true;
			}
			if (!anyButtonPressed)
			{
				ImGui::SameLine();
				ImGui::TextUnformatted("none");
			}
			static constexpr const char* switchNames[] = {
				"Center", "Up", "Up-Right", "Right", "Down-Right",
				"Down", "Down-Left", "Left", "Up-Left"
			};
			for (std::uint32_t switchIndex = 0;
				switchIndex < gamepadState.switchCount &&
				switchIndex < gamepadState.rawSwitches.size();
				++switchIndex)
			{
				const std::uint32_t position = gamepadState.rawSwitches[switchIndex];
				const char* positionName =
					position < std::size(switchNames) ? switchNames[position] : "Unknown";
				ImGui::Text("Switch %u: %s", switchIndex, positionName);
			}
		}
		ImGui::Text("Left Stick: X %.2f  Y %.2f",
			gamepadState.leftStickX, gamepadState.leftStickY);
		ImGui::Text("Brake: %s", gamepadState.brakePressed ? "On" : "Off");
		ImGui::Text("Brake binding: raw button %u",
			Tank::Input::GamepadState::BrakeButtonIndex);
	}
	ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
	ImGui::SeparatorText("Ground");
	ImGui::SliderFloat(
		"Floor Size", &m_environmentSettings.floorSizeM, 20.0f, 1000.0f, "%.0f m");
	ImGui::SliderFloat(
		"Floor Friction", &m_environmentSettings.floorFriction, 0.0f, 2.0f, "%.2f");
	ImGui::Checkbox("Grid Enabled", &m_environmentSettings.gridEnabled);
	ImGui::SliderFloat(
		"Grid Spacing", &m_environmentSettings.gridSpacingM, 0.5f, 20.0f, "%.1f m");
	if (ImGui::Button("Apply Ground & Reset"))
	{
		EnterTrackedVehicleMode();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Ground"))
	{
		SaveEnvironmentSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Ground"))
	{
		LoadEnvironmentSettings();
	}
	if (!m_environmentSettingsStatus.empty())
	{
		ImGui::TextWrapped("%s", m_environmentSettingsStatus.c_str());
	}
	ImGui::SeparatorText("Physics Settings");
	ImGui::SliderFloat(
		"Chassis Mass", &m_trackedVehicleSettings.chassisMassKg, 1000.0f, 8000.0f, "%.0f kg");

	ImGui::SeparatorText("Rolling Parameter:");

	ImGui::Checkbox("Rolling Input", &m_trackedVehicleSettings.rollingInputEnabled);
	ImGui::SliderFloat(
		"Roll Torque", &m_trackedVehicleSettings.rollTorqueNm, 20000.0f, 300000.0f, "%.0f N m");
	ImGui::SliderFloat(
		"Roll Distance", &m_trackedVehicleSettings.rollDistanceM, 0.5f, 5.0f, "%.2f m");
	ImGui::SliderFloat(
		"Torque Cutoff Angle",
		&m_trackedVehicleSettings.rollTorqueCutoffDegrees,
		45.0f,
		120.0f,
		"%.0f deg");
	ImGui::SliderFloat(
		"Stabilization Torque",
		&m_trackedVehicleSettings.rollStabilizationTorqueNm,
		0.0f,
		100000.0f,
		"%.0f N m");
	ImGui::SliderFloat(
		"Stabilization Damping",
		&m_trackedVehicleSettings.rollStabilizationDampingNms,
		0.0f,
		50000.0f,
		"%.0f N m s");

	ImGui::SeparatorText("Tank Design:");

	ImGui::SliderFloat(
		"Track Width", &m_trackedVehicleSettings.trackWidthM, 0.15f, 0.6f, "%.2f m");
	ImGui::SliderFloat(
		"Track Spacing", &m_trackedVehicleSettings.trackSpacingM, 1.8f, 3.2f, "%.2f m");
	ImGui::SliderFloat(
		"Ride Height", &m_trackedVehicleSettings.rideHeightScale, 0.5f, 1.1f, "%.2f x");
	ImGui::SliderFloat(
		"Chassis Width", &m_trackedVehicleSettings.chassisWidthM, 1.6f, 3.2f, "%.2f m");
	ImGui::SliderFloat(
		"Chassis Length", &m_trackedVehicleSettings.chassisLengthM, 3.0f, 5.5f, "%.2f m");
	ImGui::SliderFloat(
		"Wheel Radius", &m_trackedVehicleSettings.wheelRadiusM, 0.2f, 0.5f, "%.2f m");
	const char* wheelLayouts[] = { "1 + 2 + 1", "1 + 3 + 1", "1 + 4 + 1" };
	int wheelLayoutIndex = std::clamp(m_trackedVehicleSettings.roadWheelCount, 2, 4) - 2;
	if (ImGui::Combo("Wheel Layout", &wheelLayoutIndex, wheelLayouts, std::size(wheelLayouts)))
	{
		m_trackedVehicleSettings.roadWheelCount = wheelLayoutIndex + 2;
	}
	ImGui::Checkbox("Start Upside Down", &m_trackedVehicleSettings.startUpsideDown);
	if (ImGui::Button("Apply & Reset"))
	{
		ResetTrackedVehicle();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		SaveTankSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		LoadTankSettings();
	}
	if (!m_tankSettingsStatus.empty())
	{
		ImGui::TextWrapped("%s", m_tankSettingsStatus.c_str());
	}
	ImGui::SeparatorText("Simulation");
	if (ImGui::Button(m_trackedVehiclePaused ? "Resume" : "Pause"))
	{
		m_trackedVehiclePaused = !m_trackedVehiclePaused;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_trackedVehiclePaused);
	if (ImGui::Button("Step Fwd"))
	{
		m_trackedVehicleSingleStep = true;
	}
	ImGui::EndDisabled();
	if (ImGui::Button("Reset"))
	{
		ResetTrackedVehicle();
	}
	ImGui::SeparatorText("Track Input");
	ImGui::Text(
		"Analog track axes 1 / 3: %s",
		m_analogTracksConnected ? "connected" : "not connected");
	ImGui::Text(
		"Left %.2f  Right %.2f  Roll %.2f",
		m_analogLeftTrack,
		m_analogRightTrack,
		m_analogRoll);
	const Tank::Physics::TrackedDriverInput& driverInput =
		m_trackedVehicleTest.DriverInput();
	ImGui::Text(
		"SetDriverInput: Fwd %.2f  L %.2f  R %.2f  Brake %.2f",
		driverInput.forward,
		driverInput.leftRatio,
		driverInput.rightRatio,
		driverInput.brake);
	ImGui::Separator();
	ImGui::Text("Press ESC to return to the top menu.");
	ImGui::End();
}

void TankSandboxApp::UpdateTrackedVehicleInput()
{
	Tank::Physics::TankInput input;
	const Tank::Input::GamepadState& gamepadState = m_gamepad.State();
	m_analogTracksConnected = gamepadState.connected && gamepadState.axisCount >= 4;

	m_analogLeftTrack =
		m_analogTracksConnected ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[3]) : 0.0f;
	m_analogRightTrack =
		m_analogTracksConnected ? -NormalizeRawGamepadAxis(gamepadState.rawAxes[1]) : 0.0f;
	const float analogRollAxis0 =
		m_analogTracksConnected ? NormalizeRawGamepadAxis(gamepadState.rawAxes[0]) : 0.0f;
	const float analogRollAxis2 =
		m_analogTracksConnected ? NormalizeRawGamepadAxis(gamepadState.rawAxes[2]) : 0.0f;

	m_analogRoll = std::clamp((analogRollAxis0 + analogRollAxis2) * 0.5f, -1.0f, 1.0f);

	if (m_analogLeftTrack != 0.0f || m_analogRightTrack != 0.0f)
	{
		const float maximumMagnitude =
			std::max(std::abs(m_analogLeftTrack), std::abs(m_analogRightTrack));
		if (m_analogLeftTrack <= 0.0f && m_analogRightTrack <= 0.0f)
		{
			input.throttle = -maximumMagnitude;
			input.leftTrack = -m_analogLeftTrack / maximumMagnitude;
			input.rightTrack = -m_analogRightTrack / maximumMagnitude;
		}
		else
		{
			input.throttle = maximumMagnitude;
			input.leftTrack = m_analogLeftTrack / maximumMagnitude;
			input.rightTrack = m_analogRightTrack / maximumMagnitude;
		}
		input.roll = m_analogRoll != 0.0f
			? m_analogRoll
			: (m_rollLeft ? 1.0f : (m_rollRight ? -1.0f : 0.0f));
		input.brake = m_brake;
		m_trackedVehicleTest.SetInput(input);
		return;
	}

	input.throttle = m_moveForward ? 1.0f : (m_moveBackward ? -1.0f : 0.0f);
	input.roll = m_analogRoll != 0.0f
		? m_analogRoll
		: (m_rollLeft ? 1.0f : (m_rollRight ? -1.0f : 0.0f));
	input.brake = m_brake;

	if (m_turnLeft != m_turnRight)
	{
		if (input.throttle == 0.0f)
		{
			input.throttle = 1.0f;
			if (m_pivotTurnModifier)
			{
				input.leftTrack = m_turnLeft ? -1.0f : 1.0f;
				input.rightTrack = m_turnLeft ? 1.0f : -1.0f;
			}
			else
			{
				input.leftTrack = m_turnLeft ? 0.0f : 1.0f;
				input.rightTrack = m_turnLeft ? 1.0f : 0.0f;
			}
		}
		else
		{
			input.leftTrack = m_turnLeft ? 0.6f : 1.0f;
			input.rightTrack = m_turnLeft ? 1.0f : 0.6f;
		}
	}

	const bool gamepadActive =
		!m_analogTracksConnected &&
		gamepadState.connected &&
		(std::abs(gamepadState.leftStickX) > 0.05f ||
			std::abs(gamepadState.leftStickY) > 0.05f ||
			gamepadState.brakePressed);
	if (gamepadActive)
	{
		const bool keyboardBrake = input.brake;
		const float keyboardRoll = input.roll;
		input = Tank::Input::MapGamepadToTankInput(gamepadState);
		input.brake = input.brake || keyboardBrake;
		input.roll = keyboardRoll;
	}

	if (m_analogTracksConnected &&
		m_analogLeftTrack == 0.0f &&
		m_analogRightTrack == 0.0f &&
		input.throttle == 0.0f)
	{
		input.brakeAmount = 0.15f;
	}

	m_trackedVehicleTest.SetInput(input);
}

void TankSandboxApp::ResetTrackedVehicle()
{
	m_trackedVehicleTest.Initialize(m_trackedVehicleSettings, m_environmentSettings);
	m_trackedVehicleSingleStep = false;
	UpdateTrackedVehicleScene(m_trackedVehicleTest.State());
}

bool TankSandboxApp::SaveTankSettings()
{
	const std::filesystem::path path(kTankSettingsPath);
	std::error_code errorCode;
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		m_tankSettingsStatus = "Save failed: " + errorCode.message();
		return false;
	}

	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		m_tankSettingsStatus = "Save failed: cannot open file";
		return false;
	}

	output << Tank::Physics::SerializeTankSettings(m_trackedVehicleSettings);
	if (!output)
	{
		m_tankSettingsStatus = "Save failed: cannot write file";
		return false;
	}

	m_tankSettingsStatus = std::string("Saved: ") + kTankSettingsPath;
	return true;
}

bool TankSandboxApp::LoadTankSettings()
{
	std::ifstream input(kTankSettingsPath, std::ios::binary);
	if (!input)
	{
		m_tankSettingsStatus = "Load failed: no saved settings";
		return false;
	}

	const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	Tank::Physics::TankSettings loaded = m_trackedVehicleSettings;
	std::string error;
	if (!Tank::Physics::DeserializeTankSettings(json, loaded, &error))
	{
		m_tankSettingsStatus = "Load failed: " + error;
		return false;
	}

	m_trackedVehicleSettings = loaded;
	ResetTrackedVehicle();
	m_tankSettingsStatus = std::string("Loaded: ") + kTankSettingsPath;
	return true;
}

bool TankSandboxApp::SaveEnvironmentSettings()
{
	const std::filesystem::path path(kEnvironmentSettingsPath);
	std::error_code errorCode;
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		m_environmentSettingsStatus = "Save failed: " + errorCode.message();
		return false;
	}

	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		m_environmentSettingsStatus = "Save failed: cannot open file";
		return false;
	}

	output << Tank::Physics::SerializePhysicsEnvironmentSettings(m_environmentSettings);
	if (!output)
	{
		m_environmentSettingsStatus = "Save failed: cannot write file";
		return false;
	}

	m_environmentSettingsStatus = std::string("Saved: ") + kEnvironmentSettingsPath;
	return true;
}

bool TankSandboxApp::LoadEnvironmentSettings()
{
	std::ifstream input(kEnvironmentSettingsPath, std::ios::binary);
	if (!input)
	{
		m_environmentSettingsStatus = "Load failed: no saved settings";
		return false;
	}

	const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	Tank::Physics::PhysicsEnvironmentSettings loaded = m_environmentSettings;
	std::string error;
	if (!Tank::Physics::DeserializePhysicsEnvironmentSettings(json, loaded, &error))
	{
		m_environmentSettingsStatus = "Load failed: " + error;
		return false;
	}

	m_environmentSettings = loaded;
	EnterTrackedVehicleMode();
	m_environmentSettingsStatus = std::string("Loaded: ") + kEnvironmentSettingsPath;
	return true;
}

void TankSandboxApp::EnterTrackedVehicleMode()
{
	m_trackedVehicleSceneBuilder.Clear();

	uint32_t floorMaterial = 0;
	if (m_environmentSettings.gridEnabled)
	{
		constexpr uint32_t gridTextureSize = 128;
		const std::vector<uint8_t> gridPixels = CreateGroundGridTexture(gridTextureSize);
		const uint32_t gridTexture = m_trackedVehicleSceneBuilder.AddTextureRGBA8(
			gridTextureSize,
			gridTextureSize,
			gridPixels);
		const float gridSpacingM = std::max(m_environmentSettings.gridSpacingM, 0.5f);
		const float gridRepeat = m_environmentSettings.floorSizeM / gridSpacingM;
		floorMaterial = m_trackedVehicleSceneBuilder.AddTexturedMaterial(
			gridTexture,
			{ gridRepeat, gridRepeat });
	}
	else
	{
		floorMaterial =
			m_trackedVehicleSceneBuilder.AddSolidColorMaterial(80, 80, 80, 255);
	}
	const uint32_t hullMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(55, 95, 65, 255);
	const uint32_t upperMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(70, 120, 80, 255);
	const uint32_t leftTrackMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(40, 40, 45, 255);
	const uint32_t rightTrackMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(50, 50, 55, 255);
	const uint32_t markerMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(255, 60, 60, 255);
	m_trackedVehicleModel.wheelContactMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(70, 200, 90, 255);
	m_trackedVehicleModel.wheelAirborneMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(230, 140, 40, 255);

	m_trackedVehicleSceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);

	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(
			m_environmentSettings.floorSizeM,
			0.2f,
			m_environmentSettings.floorSizeM) *
			XMMatrixTranslation(0.0f, -0.1f, 0.0f),
		floorMaterial);

	m_trackedVehicleModel.lowerHull = 1;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(2.16f, 0.5f, 3.5f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f),
		hullMaterial);

	m_trackedVehicleModel.upperStructure = 2;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.25f, 2.0f) * XMMatrixTranslation(0.0f, 2.375f, 0.3f),
		upperMaterial);

	m_trackedVehicleModel.lowerStructure = 3;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.25f, 2.0f) * XMMatrixTranslation(0.0f, 1.625f, 0.3f),
		upperMaterial);

	m_trackedVehicleModel.leftTrack = 4;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, 4.0f) *
		XMMatrixTranslation(-0.5f * m_trackedVehicleSettings.trackSpacingM, 2.0f, 0.0f),
		leftTrackMaterial);

	m_trackedVehicleModel.rightTrack = 5;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, 4.0f) *
		XMMatrixTranslation(0.5f * m_trackedVehicleSettings.trackSpacingM, 2.0f, 0.0f),
		rightTrackMaterial);

	m_trackedVehicleModel.forwardMarker = 6;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(0.3f, 0.3f, 0.3f) * XMMatrixTranslation(0.0f, 2.0f, 2.5f),
		markerMaterial);

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		m_trackedVehicleModel.wheels[static_cast<size_t>(i)] =
			m_trackedVehicleSceneBuilder.GetScene().instances.size();
		m_trackedVehicleSceneBuilder.AddInstance(
			XMMatrixScaling(0.0f, 0.0f, 0.0f),
			m_trackedVehicleModel.wheelAirborneMaterial);
	}

	Engine::CameraState camera;
	camera.pos = { 0.0f, 10.0f, -16.0f };
	camera.gazePoint = { 0.0f, 0.8f, 0.0f };
	camera.fov = 35.0f;
	camera.nearZ = 0.1f;
	camera.farZ = 10000.0f;
	m_trackedVehicleSceneBuilder.SetCamera(camera);
	ActivateOrbitCamera(m_trackedVehicleSceneBuilder.GetScene(), { 0.0f, 0.8f, 0.0f });

	m_trackedVehicleTest.Initialize(m_trackedVehicleSettings, m_environmentSettings);
	UpdateTrackedVehicleScene(m_trackedVehicleTest.State());
	m_trackedVehiclePaused = false;
	m_trackedVehicleSingleStep = false;
	m_sceneRenderer.SetScene(m_trackedVehicleSceneBuilder.GetScene());
	m_sceneRenderer.ReloadSceneResources(m_trackedVehicleSceneBuilder.GetScene());
	m_sceneRenderer.SetDisplayInstanceCount(
		static_cast<int>(m_trackedVehicleSceneBuilder.GetScene().instances.size()));
	m_appMode = AppMode::PhysicsTrackedVehicle;
}

void TankSandboxApp::UpdateTrackedVehicleScene(const Tank::Physics::TrackedVehicleTestState& state)
{
	Engine::Scene& scene = m_trackedVehicleSceneBuilder.GetScene();
	const XMVECTOR rotation = XMVectorSet(
		state.bodyRotation.x, state.bodyRotation.y, state.bodyRotation.z, state.bodyRotation.w);
	const XMMATRIX bodyTransform =
		XMMatrixRotationQuaternion(rotation) *
		XMMatrixTranslation(state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
	const float chassisWidth = m_trackedVehicleSettings.chassisWidthM;
	const float chassisLength = m_trackedVehicleSettings.chassisLengthM;

	struct Part { size_t index; XMMATRIX localTransform; };
	const Part parts[] = {
		{ m_trackedVehicleModel.lowerHull,
			XMMatrixScaling(0.9f * chassisWidth, 0.5f, 0.875f * chassisLength) },
		{ m_trackedVehicleModel.upperStructure,
			XMMatrixScaling(0.6f * chassisWidth, 0.25f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, 0.375f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.lowerStructure,
			XMMatrixScaling(0.6f * chassisWidth, 0.25f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, -0.375f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.leftTrack,
			XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, chassisLength) *
				XMMatrixTranslation(-0.5f * m_trackedVehicleSettings.trackSpacingM, 0.0f, 0.0f) },
		{ m_trackedVehicleModel.rightTrack,
			XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, chassisLength) *
				XMMatrixTranslation(0.5f * m_trackedVehicleSettings.trackSpacingM, 0.0f, 0.0f) },
		{ m_trackedVehicleModel.forwardMarker,
			XMMatrixScaling(0.3f, 0.3f, 0.3f) *
				XMMatrixTranslation(0.0f, 0.0f, 0.625f * chassisLength) },
	};
	for (const Part& part : parts)
	{
		Engine::InstanceData& inst = scene.instances[part.index];
		inst.prevWorld = inst.world;
		const XMMATRIX world = part.localTransform * bodyTransform;
		XMStoreFloat4x4(&inst.world, XMMatrixTranspose(world));
	}

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		Engine::InstanceData& inst =
			scene.instances[m_trackedVehicleModel.wheels[static_cast<size_t>(i)]];
		inst.prevWorld = inst.world;

		if (i < state.wheelCount)
		{
			const Tank::Physics::TrackedWheelState& wheel = state.wheels[static_cast<size_t>(i)];
			const XMVECTOR wheelRotation = XMVectorSet(
				wheel.transform.rotation.x,
				wheel.transform.rotation.y,
				wheel.transform.rotation.z,
				wheel.transform.rotation.w);
			const XMMATRIX wheelWorld =
				XMMatrixScaling(
					2.0f * m_trackedVehicleSettings.wheelRadiusM,
					m_trackedVehicleSettings.trackWidthM,
					2.0f * m_trackedVehicleSettings.wheelRadiusM) *
				XMMatrixRotationQuaternion(wheelRotation) *
				XMMatrixTranslation(
					wheel.transform.position.x,
					wheel.transform.position.y,
					wheel.transform.position.z);
			XMStoreFloat4x4(&inst.world, XMMatrixTranspose(wheelWorld));
			inst.materialId = wheel.hasContact
				? m_trackedVehicleModel.wheelContactMaterial
				: m_trackedVehicleModel.wheelAirborneMaterial;
		}
		else
		{
			XMStoreFloat4x4(
				&inst.world,
				XMMatrixTranspose(XMMatrixScaling(0.0f, 0.0f, 0.0f)));
			inst.materialId = m_trackedVehicleModel.wheelAirborneMaterial;
		}
	}

	m_debugCameraController.SetObjectViewerState(
		m_debugCameraController.ObjectViewerYaw(),
		m_debugCameraController.ObjectViewerPitch(),
		m_debugCameraController.ObjectViewerDistance(),
		{
		state.bodyPosition.x,
		state.bodyPosition.y + 0.5f,
		state.bodyPosition.z });
	m_sceneRenderer.SetScene(scene);
}

void TankSandboxApp::ApplyTrackedVehicleCameraPreset(const XMFLOAT3& offset)
{
	const Tank::Physics::TrackedVehicleTestState& state = m_trackedVehicleTest.State();
	const XMFLOAT3 pivot = {
		state.bodyPosition.x,
		state.bodyPosition.y + 0.5f,
		state.bodyPosition.z
	};
	Engine::Scene& scene = m_trackedVehicleSceneBuilder.GetScene();
	scene.camera.pos = {
		pivot.x + offset.x,
		pivot.y + offset.y,
		pivot.z + offset.z
	};
	scene.camera.gazePoint = pivot;
	ActivateOrbitCamera(scene, pivot);
	m_sceneRenderer.SetCamera(scene.camera);
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
		return &m_boxDropSceneBuilder.GetScene().camera;
	case AppMode::PhysicsTrackedVehicle:
		return &m_trackedVehicleSceneBuilder.GetScene().camera;
	case AppMode::TopMenu:
		return nullptr;
	}

	return nullptr;
}
