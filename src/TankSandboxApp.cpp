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
#include "Physics/TestObstacleLayout.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/CameraSettings.h"
#include "Rendering/TankVisualSettingsJson.h"
#include "Input/GamepadState.h"

using namespace DirectX;

namespace
{
	constexpr const char* kRendererSettingsPath = "Config/renderer_debug.json";
	constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";

	bool IsPending(float value, float appliedValue)
	{
		return std::abs(value - appliedValue) > 0.0001f;
	}

	bool SliderFloatWithPendingColor(
		const char* label,
		float* value,
		float min,
		float max,
		float delta,
		float defaultValue,
		const char* format,
		bool pending)
	{
		if (pending)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
		}
		const bool changed = ImGuiWidgets::SliderFloatWithControls(
			label, value, min, max, delta, defaultValue, format);
		if (pending)
		{
			ImGui::PopStyleColor();
		}
		return changed;
	}

	bool SliderIntWithPendingColor(
		const char* label,
		int* value,
		int min,
		int max,
		int delta,
		int defaultValue,
		const char* format,
		bool pending)
	{
		if (pending)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
		}
		const bool changed = ImGuiWidgets::SliderIntWithControls(
			label, value, min, max, delta, defaultValue, format);
		if (pending)
		{
			ImGui::PopStyleColor();
		}
		return changed;
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
		else if (arg == L"--physics-debug-overlay")
		{
			m_physicsDebugOverlay = true;
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
	if (m_tankSettingsAutoLoad)
	{
		LoadTankSettings(false);
	}
	if (m_tankVisualSettingsAutoLoad)
	{
		LoadTankVisualSettings(false);
	}
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
			m_boxDropPresenter.Clear();
			m_trackedVehiclePresenter.Clear();
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
		const Tank::Physics::BoxDropState state = m_boxDropTest.Step(kPhysicsFixedDt);
		m_boxDropPresenter.UpdateScene(state);
		m_sceneRenderer.SetScene(m_boxDropPresenter.GetScene());
	}
	else if (m_appMode == AppMode::PhysicsTrackedVehicle)
	{
		m_gamepad.Poll();
		{
			const Tank::Input::GamepadState& gp = m_gamepad.State();
			Tank::App::CameraSettingsStore store(m_cameraController.SelectedSlot());
			m_cameraController.UpdateButtonStates(
				gp.connected && gp.buttonCount > 4 && gp.rawButtons[4],
				gp.connected && gp.buttonCount > 7 && gp.rawButtons[7],
				store);
		}
		UpdateTrackedVehicleInput();
		if (!m_trackedVehiclePaused || m_trackedVehicleSingleStep)
		{
			const Tank::Physics::TrackedVehicleTestState state = m_trackedVehicleTest.Step(kPhysicsFixedDt);
			m_trackedVehiclePresenter.UpdateScene(
				state,
				m_trackedVehicleSettings,
				m_tankVisualSettings,
				m_trackShoeDisplay,
				m_showTrackProxies,
				m_physicsDebugOverlay);
			m_sceneRenderer.SetScene(m_trackedVehiclePresenter.GetScene());
			m_trackedVehicleSingleStep = false;
		}
		if (Engine::CameraState* camera = ActiveCamera())
		{
			m_cameraController.UpdateFollowCamera(
				m_trackedVehicleTest.State(),
				kPhysicsFixedDt,
				*camera);
		}
	}
	if (Engine::CameraState* camera = ActiveCamera())
	{
		m_cameraController.UpdateTransition(kPhysicsFixedDt, *camera);
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

	ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - 950, 10), ImGuiCond_FirstUseEver);
	DrawCameraUi();

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
	ImGui::SeparatorText("Save Slot");
	for (int slot = 0; slot < 3; ++slot)
	{
		if (slot > 0)
		{
			ImGui::SameLine();
		}
		const std::string label = std::to_string(slot + 1);
		if (ImGui::RadioButton(
			label.c_str(),
			m_cameraController.SelectedSlot() == slot))
		{
			Tank::App::CameraSettingsStore store(m_cameraController.SelectedSlot());
			m_cameraController.SelectSlot(slot, m_cameraController.AutoLoad(), store);
		}
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Debug", m_cameraController.SelectedSlot() == 3))
	{
		Tank::App::CameraSettingsStore store(m_cameraController.SelectedSlot());
		m_cameraController.SelectSlot(3, m_cameraController.AutoLoad(), store);
	}
	ImGui::SameLine();
	{
		bool autoLoad = m_cameraController.AutoLoad();
		if (ImGui::Checkbox("AutoLoad", &autoLoad))
		{
			m_cameraController.SetAutoLoad(autoLoad);
		}
	}
	if (ImGui::Button("Save Camera"))
	{
		SaveCameraSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Camera"))
	{
		LoadCameraSettings();
	}
	if (!m_cameraController.Status().empty())
	{
		ImGui::TextWrapped("%s", m_cameraController.Status().c_str());
	}
	if (m_appMode == AppMode::PhysicsTrackedVehicle)
	{
		bool follow = m_cameraController.FollowEnabled();
		if (ImGui::Checkbox("Follow Tank", &follow))
		{
			m_cameraController.SetFollowEnabled(follow);
			m_cameraController.ResetFollowState();
			if (!follow)
			{
				const Tank::Physics::TrackedVehicleTestState& state =
					m_trackedVehicleTest.State();
			ActivateOrbitCamera(
				m_trackedVehiclePresenter.GetScene(),
				{
					state.bodyPosition.x,
					state.bodyPosition.y + 0.5f,
					state.bodyPosition.z });
			}
		}
		ImGui::BeginDisabled(!m_cameraController.FollowEnabled());
		{
			float dist = m_cameraController.FollowDistance();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Follow Distance", &dist, 4.0f, 250.0f, 0.5f, 16.0f))
			{
				m_cameraController.SetFollowDistance(dist);
			}
		}
		{
			float val = m_cameraController.LookDownDegrees();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Look Down Angle", &val, 0.0f, 89.0f, 1.0f, 25.0f, "%.1f deg"))
			{
				m_cameraController.SetLookDownDegrees(val);
			}
		}
		{
			float val = m_cameraController.PositionSpeed();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Position Speed", &val, 0.5f, 20.0f, 0.5f, 5.0f))
			{
				m_cameraController.SetPositionSpeed(val);
			}
		}
		{
			float val = m_cameraController.RotationSpeed();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Rotation Speed", &val, 0.5f, 20.0f, 0.5f, 8.0f))
			{
				m_cameraController.SetRotationSpeed(val);
			}
		}
		{
			float val = m_cameraController.YawSpeedLimitDegrees();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Yaw Speed Limit", &val, 15.0f, 720.0f, 15.0f, 180.0f, "%.0f deg/s"))
			{
				m_cameraController.SetYawSpeedLimitDegrees(val);
			}
		}
		{
			float val = m_cameraController.YawDamping();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Yaw Damping", &val, 0.5f, 30.0f, 0.5f, 8.0f))
			{
				m_cameraController.SetYawDamping(val);
			}
		}
		{
			float val = m_cameraController.Damping();
			if (ImGuiWidgets::SliderFloatWithControls(
				"Damping", &val, 0.1f, 2.0f, 0.05f, 1.0f))
			{
				m_cameraController.SetDamping(val);
			}
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(m_cameraController.FollowEnabled());
		ImGui::SeparatorText("Angle");
		if (ImGui::Button("Rear High"))
		{
			const Tank::Physics::TrackedVehicleTestState& state =
				m_trackedVehicleTest.State();
			m_cameraController.ApplyCameraPreset(
				{ 0.0f, 9.2f, -16.0f }, state,
				m_trackedVehiclePresenter.GetScene().camera);
			ActivateOrbitCamera(
				m_trackedVehiclePresenter.GetScene(),
				{ state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
		}
		ImGui::SameLine();
		if (ImGui::Button("Rear Quarter"))
		{
			const Tank::Physics::TrackedVehicleTestState& state =
				m_trackedVehicleTest.State();
			m_cameraController.ApplyCameraPreset(
				{ 10.0f, 7.0f, -14.0f }, state,
				m_trackedVehiclePresenter.GetScene().camera);
			ActivateOrbitCamera(
				m_trackedVehiclePresenter.GetScene(),
				{ state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
		}
		if (ImGui::Button("Side High"))
		{
			const Tank::Physics::TrackedVehicleTestState& state =
				m_trackedVehicleTest.State();
			m_cameraController.ApplyCameraPreset(
				{ 16.0f, 6.0f, 0.0f }, state,
				m_trackedVehiclePresenter.GetScene().camera);
			ActivateOrbitCamera(
				m_trackedVehiclePresenter.GetScene(),
				{ state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
		}
		ImGui::SameLine();
		if (ImGui::Button("Top Rear"))
		{
			const Tank::Physics::TrackedVehicleTestState& state =
				m_trackedVehicleTest.State();
			m_cameraController.ApplyCameraPreset(
				{ 0.0f, 18.0f, -4.0f }, state,
				m_trackedVehiclePresenter.GetScene().camera);
			ActivateOrbitCamera(
				m_trackedVehiclePresenter.GetScene(),
				{ state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
		}
		ImGui::EndDisabled();
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
		const bool fovChanged =
			ImGui::SliderFloat("FOV Y", &camera->fov, 20.0f, 120.0f, "%.1f deg");
		changed |= fovChanged;
		if (fovChanged)
		{
			m_cameraController.ResetFollowState();
		}
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
	m_cameraController.EnsureSlotLoaded(
		m_cameraController.SelectedSlot(), store);
	return true;
}

bool TankSandboxApp::LoadCameraSettings()
{
	Engine::CameraState* camera = ActiveCamera();
	if (camera == nullptr)
	{
		return false;
	}
	Tank::App::CameraSettingsStore store(m_cameraController.SelectedSlot());
	if (!m_cameraController.EnsureSlotLoaded(
		m_cameraController.SelectedSlot(), store))
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
	m_boxDropPresenter.BuildScene();
	Engine::Scene& scene = m_boxDropPresenter.GetScene();
	ActivateOrbitCamera(scene, { 0.0f, 1.0f, 0.0f });

	m_boxDropTest.Initialize();

	m_sceneRenderer.SetScene(scene);
	m_sceneRenderer.ReloadSceneResources(scene);
	m_sceneRenderer.SetDisplayInstanceCount(static_cast<int>(scene.instances.size()));

	m_appMode = AppMode::PhysicsBoxDrop;
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
		m_boxDropPresenter.UpdateScene(m_boxDropTest.State());
		m_sceneRenderer.SetScene(m_boxDropPresenter.GetScene());
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
	if (ImGui::Checkbox("Physics Debug Overlay", &m_physicsDebugOverlay))
	{
		m_trackedVehiclePresenter.UpdateScene(
			state,
			m_trackedVehicleSettings,
			m_tankVisualSettings,
			m_trackShoeDisplay,
			m_showTrackProxies,
			m_physicsDebugOverlay);
		m_sceneRenderer.SetScene(m_trackedVehiclePresenter.GetScene());
	}
	if (m_physicsDebugOverlay)
	{
		ImGui::TextUnformatted("Cyan: suspension  Green/Orange: contact  Yellow: normal");
	}
	ImGui::Text("Controls: W/S drive, A/D skid turn, Shift+A/D pivot");
	ImGui::Text("Q/E roll, Space brake");
	const Tank::Input::GamepadState& gamepadState = m_gamepad.State();
	if (ImGui::CollapsingHeader("Gamepad"))
	{
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
	}
	ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
	if (ImGui::CollapsingHeader("Ground"))
	{
		SliderFloatWithPendingColor(
			"Floor Size",
			&m_environmentSettings.floorSizeM,
			20.0f,
			1000.0f,
			10.0f,
			200.0f,
			"%.0f m",
			IsPending(m_environmentSettings.floorSizeM, m_appliedEnvironmentSettings.floorSizeM));
		SliderFloatWithPendingColor(
			"Floor Friction",
			&m_environmentSettings.floorFriction,
			0.0f,
			2.0f,
			0.05f,
			0.6f,
			"%.2f",
			IsPending(
				m_environmentSettings.floorFriction,
				m_appliedEnvironmentSettings.floorFriction));
		ImGui::Checkbox("Grid Enabled", &m_environmentSettings.gridEnabled);
		SliderFloatWithPendingColor(
			"Grid Spacing",
			&m_environmentSettings.gridSpacingM,
			0.5f,
			20.0f,
			0.5f,
			5.0f,
			"%.1f m",
			IsPending(m_environmentSettings.gridSpacingM, m_appliedEnvironmentSettings.gridSpacingM));
		SliderIntWithPendingColor(
			"Obstacle Count",
			&m_environmentSettings.obstacleCount,
			0,
			100,
			1,
			20,
			"%d",
			m_environmentSettings.obstacleCount != m_appliedEnvironmentSettings.obstacleCount);
		SliderIntWithPendingColor(
			"Obstacle Seed",
			&m_environmentSettings.obstacleSeed,
			0,
			9999,
			1,
			1,
			"%d",
			m_environmentSettings.obstacleSeed != m_appliedEnvironmentSettings.obstacleSeed);
		SliderFloatWithPendingColor(
			"Obstacle Area",
			&m_environmentSettings.obstacleAreaSizeM,
			20.0f,
			500.0f,
			10.0f,
			100.0f,
			"%.0f m",
			IsPending(
				m_environmentSettings.obstacleAreaSizeM,
				m_appliedEnvironmentSettings.obstacleAreaSizeM));
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
	}
	ImGui::SeparatorText("Physics Settings");
	SliderFloatWithPendingColor(
		"Chassis Mass",
		&m_trackedVehicleSettings.chassisMassKg,
		1000.0f,
		8000.0f,
		100.0f,
		4000.0f,
		"%.0f kg",
		IsPending(
			m_trackedVehicleSettings.chassisMassKg,
			m_appliedTrackedVehicleSettings.chassisMassKg));
	ImGui::Checkbox("Neutral Brake", &m_trackedVehicleSettings.neutralBrakeEnabled);
	SliderFloatWithPendingColor(
		"Neutral Brake Strength",
		&m_trackedVehicleSettings.neutralBrakeAmount,
		0.0f,
		1.0f,
		0.05f,
		0.15f,
		"%.2f",
		false);

	ImGui::SeparatorText("Rolling Parameter:");

	ImGui::Checkbox("Rolling Input", &m_trackedVehicleSettings.rollingInputEnabled);
	SliderFloatWithPendingColor(
		"Roll Torque",
		&m_trackedVehicleSettings.rollTorqueNm,
		20000.0f,
		300000.0f,
		5000.0f,
		120000.0f,
		"%.0f N m",
		IsPending(m_trackedVehicleSettings.rollTorqueNm, m_appliedTrackedVehicleSettings.rollTorqueNm));
	SliderFloatWithPendingColor(
		"Roll Distance",
		&m_trackedVehicleSettings.rollDistanceM,
		0.5f,
		5.0f,
		0.1f,
		2.4f,
		"%.2f m",
		IsPending(m_trackedVehicleSettings.rollDistanceM, m_appliedTrackedVehicleSettings.rollDistanceM));
	SliderFloatWithPendingColor(
		"Torque Cutoff Angle",
		&m_trackedVehicleSettings.rollTorqueCutoffDegrees,
		45.0f,
		120.0f,
		5.0f,
		90.0f,
		"%.0f deg",
		IsPending(
			m_trackedVehicleSettings.rollTorqueCutoffDegrees,
			m_appliedTrackedVehicleSettings.rollTorqueCutoffDegrees));
	SliderFloatWithPendingColor(
		"Stabilization Torque",
		&m_trackedVehicleSettings.rollStabilizationTorqueNm,
		0.0f,
		100000.0f,
		5000.0f,
		30000.0f,
		"%.0f N m",
		IsPending(
			m_trackedVehicleSettings.rollStabilizationTorqueNm,
			m_appliedTrackedVehicleSettings.rollStabilizationTorqueNm));
	SliderFloatWithPendingColor(
		"Stabilization Damping",
		&m_trackedVehicleSettings.rollStabilizationDampingNms,
		0.0f,
		50000.0f,
		1000.0f,
		10000.0f,
		"%.0f N m s",
		IsPending(
			m_trackedVehicleSettings.rollStabilizationDampingNms,
			m_appliedTrackedVehicleSettings.rollStabilizationDampingNms));

	ImGui::SeparatorText("Tank Design:");

	if (ImGui::CollapsingHeader("Body Material"))
	{
		auto drawMaterial = [](const char* label, Tank::Rendering::BodyMaterialSettings& material)
		{
			bool changed = false;
			if (ImGui::TreeNode(label))
			{
				changed |= ImGui::ColorEdit3("Albedo", &material.albedo.r);
				changed |= ImGuiWidgets::SliderFloatWithControls(
					"Roughness", &material.roughness, 0.04f, 1.0f, 0.02f, 0.8f);
				changed |= ImGuiWidgets::SliderFloatWithControls(
					"Metallic", &material.metallic, 0.0f, 1.0f, 0.05f, 0.0f);
				changed |= ImGuiWidgets::SliderFloatWithControls(
					"Ambient Occlusion",
					&material.ambientOcclusion,
					0.0f,
					1.0f,
					0.05f,
					1.0f);
				changed |= ImGuiWidgets::SliderFloatWithControls(
					"Emissive", &material.emissive, 0.0f, 4.0f, 0.1f, 0.0f);
				ImGui::TreePop();
			}
			return changed;
		};
		bool materialChanged = false;
		materialChanged |= drawMaterial("Hull Upper", m_tankVisualSettings.hullUpper);
		materialChanged |= drawMaterial("Hull Lower", m_tankVisualSettings.hullLower);
		materialChanged |= drawMaterial(
			"Structure Upper", m_tankVisualSettings.structureUpper);
		materialChanged |= drawMaterial(
			"Structure Lower", m_tankVisualSettings.structureLower);
		materialChanged |= drawMaterial("Wheels", m_tankVisualSettings.wheels);
		materialChanged |= ImGui::Checkbox(
			"Color Wheels by Contact",
			&m_tankVisualSettings.colorWheelsByContact);
		ImGui::BeginDisabled(!m_tankVisualSettings.colorWheelsByContact);
		materialChanged |= drawMaterial(
			"Contacted Wheels",
			m_tankVisualSettings.contactedWheels);
		ImGui::EndDisabled();
		materialChanged |= drawMaterial("Track Shoes", m_tankVisualSettings.trackShoes);
		materialChanged |= drawMaterial(
			"Track Proxies", m_tankVisualSettings.trackProxies);
		materialChanged |= drawMaterial(
			"Forward Marker", m_tankVisualSettings.forwardMarker);
		m_tankVisualMaterialApplyPending |= materialChanged;
		if (m_tankVisualMaterialApplyPending && !ImGui::IsAnyItemActive())
		{
			ApplyTrackedVehicleMaterials();
			m_tankVisualMaterialApplyPending = false;
		}
		if (ImGui::Button("Save Visual"))
		{
			SaveTankVisualSettings();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Visual"))
		{
			LoadTankVisualSettings();
		}
		ImGui::SameLine();
		ImGui::Checkbox("AutoLoad##TankVisual", &m_tankVisualSettingsAutoLoad);
		if (!m_tankVisualSettingsStatus.empty())
		{
			ImGui::TextWrapped("%s", m_tankVisualSettingsStatus.c_str());
		}
	}
	if (ImGui::Checkbox("Track Shoe Display", &m_trackShoeDisplay))
	{
		m_trackedVehiclePresenter.UpdateScene(
			state,
			m_trackedVehicleSettings,
			m_tankVisualSettings,
			m_trackShoeDisplay,
			m_showTrackProxies,
			m_physicsDebugOverlay);
		m_sceneRenderer.SetScene(m_trackedVehiclePresenter.GetScene());
	}
	if (ImGui::Checkbox("Show Track Proxies", &m_showTrackProxies))
	{
		m_trackedVehiclePresenter.UpdateScene(
			state,
			m_trackedVehicleSettings,
			m_tankVisualSettings,
			m_trackShoeDisplay,
			m_showTrackProxies,
			m_physicsDebugOverlay);
		m_sceneRenderer.SetScene(m_trackedVehiclePresenter.GetScene());
	}
	SliderFloatWithPendingColor(
		"Track Width", &m_trackedVehicleSettings.trackWidthM, 0.15f, 0.6f, 0.01f, 0.3f, "%.2f m",
		IsPending(m_trackedVehicleSettings.trackWidthM, m_appliedTrackedVehicleSettings.trackWidthM));
	SliderFloatWithPendingColor(
		"Track Spacing", &m_trackedVehicleSettings.trackSpacingM, 1.8f, 3.2f, 0.1f, 2.4f, "%.2f m",
		IsPending(m_trackedVehicleSettings.trackSpacingM, m_appliedTrackedVehicleSettings.trackSpacingM));
	ImGui::SeparatorText("Turn Traction:");
	SliderFloatWithPendingColor(
		"Stationary Inner Track Ratio",
		&m_trackedVehicleSettings.stationaryTurnInnerTrackRatio,
		0.0f,
		1.0f,
		0.05f,
		0.0f,
		"%.2f x",
		IsPending(
			m_trackedVehicleSettings.stationaryTurnInnerTrackRatio,
			m_appliedTrackedVehicleSettings.stationaryTurnInnerTrackRatio));
	SliderFloatWithPendingColor(
		"Stationary Left Track",
		&m_trackedVehicleSettings.stationaryTurnLeftTraction,
		0.0f,
		1.0f,
		0.05f,
		1.0f,
		"%.2f x",
		IsPending(
			m_trackedVehicleSettings.stationaryTurnLeftTraction,
			m_appliedTrackedVehicleSettings.stationaryTurnLeftTraction));
	SliderFloatWithPendingColor(
		"Stationary Right Track",
		&m_trackedVehicleSettings.stationaryTurnRightTraction,
		0.0f,
		1.0f,
		0.05f,
		1.0f,
		"%.2f x",
		IsPending(
			m_trackedVehicleSettings.stationaryTurnRightTraction,
			m_appliedTrackedVehicleSettings.stationaryTurnRightTraction));
	SliderFloatWithPendingColor(
		"Pivot Left Track",
		&m_trackedVehicleSettings.pivotTurnLeftTraction,
		0.0f,
		1.0f,
		0.05f,
		1.0f,
		"%.2f x",
		IsPending(
			m_trackedVehicleSettings.pivotTurnLeftTraction,
			m_appliedTrackedVehicleSettings.pivotTurnLeftTraction));
	SliderFloatWithPendingColor(
		"Pivot Right Track",
		&m_trackedVehicleSettings.pivotTurnRightTraction,
		0.0f,
		1.0f,
		0.05f,
		1.0f,
		"%.2f x",
		IsPending(
			m_trackedVehicleSettings.pivotTurnRightTraction,
			m_appliedTrackedVehicleSettings.pivotTurnRightTraction));
	ImGui::SeparatorText("Body Yaw:");
	SliderFloatWithPendingColor(
		"Yaw Speed Limit",
		&m_trackedVehicleSettings.yawSpeedLimitDegrees,
		15.0f,
		720.0f,
		5.0f,
		720.0f,
		"%.0f deg/s",
		IsPending(
			m_trackedVehicleSettings.yawSpeedLimitDegrees,
			m_appliedTrackedVehicleSettings.yawSpeedLimitDegrees));
	SliderFloatWithPendingColor(
		"Yaw Damping",
		&m_trackedVehicleSettings.yawDamping,
		0.0f,
		30.0f,
		0.5f,
		0.0f,
		"%.1f /s",
		IsPending(
			m_trackedVehicleSettings.yawDamping,
			m_appliedTrackedVehicleSettings.yawDamping));
	SliderFloatWithPendingColor(
		"Ride Height", &m_trackedVehicleSettings.rideHeightScale, 0.5f, 1.1f, 0.05f, 0.8f, "%.2f x",
		IsPending(m_trackedVehicleSettings.rideHeightScale, m_appliedTrackedVehicleSettings.rideHeightScale));
	SliderFloatWithPendingColor(
		"Chassis Width", &m_trackedVehicleSettings.chassisWidthM, 1.6f, 3.2f, 0.1f, 2.4f, "%.2f m",
		IsPending(m_trackedVehicleSettings.chassisWidthM, m_appliedTrackedVehicleSettings.chassisWidthM));
	SliderFloatWithPendingColor(
		"Chassis Length", &m_trackedVehicleSettings.chassisLengthM, 3.0f, 5.5f, 0.1f, 4.0f, "%.2f m",
		IsPending(m_trackedVehicleSettings.chassisLengthM, m_appliedTrackedVehicleSettings.chassisLengthM));
	SliderFloatWithPendingColor(
		"End Wheel Radius",
		&m_trackedVehicleSettings.endWheelRadiusM,
		0.2f,
		0.6f,
		0.01f,
		0.4f,
		"%.2f m",
		IsPending(
			m_trackedVehicleSettings.endWheelRadiusM,
			m_appliedTrackedVehicleSettings.endWheelRadiusM));
	SliderFloatWithPendingColor(
		"Road Wheel Radius",
		&m_trackedVehicleSettings.roadWheelRadiusM,
		0.2f,
		0.5f,
		0.01f,
		0.3f,
		"%.2f m",
		IsPending(
			m_trackedVehicleSettings.roadWheelRadiusM,
			m_appliedTrackedVehicleSettings.roadWheelRadiusM));
	const char* wheelLayouts[] = { "1 + 2 + 1", "1 + 3 + 1", "1 + 4 + 1" };
	int wheelLayoutIndex = std::clamp(m_trackedVehicleSettings.roadWheelCount, 2, 4) - 2;
	if (ImGui::Combo("Wheel Layout", &wheelLayoutIndex, wheelLayouts, std::size(wheelLayouts)))
	{
		m_trackedVehicleSettings.roadWheelCount = wheelLayoutIndex + 2;
	}
	SliderFloatWithPendingColor(
		"End Wheel Offset",
		&m_trackedVehicleSettings.endWheelOffsetM,
		0.0f,
		1.0f,
		0.05f,
		0.0f,
		"%.2f m",
		IsPending(
			m_trackedVehicleSettings.endWheelOffsetM,
			m_appliedTrackedVehicleSettings.endWheelOffsetM));
	if (m_trackedVehicleSettings.roadWheelCount == 2)
	{
		SliderFloatWithPendingColor(
			"Middle Wheel Offset",
			&m_trackedVehicleSettings.twoRoadWheelOffsetM,
			0.1f,
			2.0f,
			0.05f,
			0.67f,
			"%.2f m",
			IsPending(
				m_trackedVehicleSettings.twoRoadWheelOffsetM,
				m_appliedTrackedVehicleSettings.twoRoadWheelOffsetM));
	}
	else if (m_trackedVehicleSettings.roadWheelCount == 3)
	{
		SliderFloatWithPendingColor(
			"Middle Wheel Offset",
			&m_trackedVehicleSettings.threeRoadWheelOffsetM,
			0.1f,
			2.0f,
			0.05f,
			1.0f,
			"%.2f m",
			IsPending(
				m_trackedVehicleSettings.threeRoadWheelOffsetM,
				m_appliedTrackedVehicleSettings.threeRoadWheelOffsetM));
	}
	ImGui::Checkbox("Start Upside Down", &m_trackedVehicleSettings.startUpsideDown);
	ImGui::TextUnformatted("Save Slot");
	ImGui::SameLine();
	for (int slot = 0; slot < 3; ++slot)
	{
		if (slot > 0)
		{
			ImGui::SameLine();
		}
		const std::string label = std::to_string(slot + 1);
		if (ImGui::RadioButton(label.c_str(), m_tankSettingsSlot == slot))
		{
			m_tankSettingsSlot = slot;
			if (m_tankSettingsAutoLoad)
			{
				LoadTankSettings();
			}
		}
	}
	ImGui::SameLine();
	ImGui::Checkbox("AutoLoad##TankSettings", &m_tankSettingsAutoLoad);
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
	const bool brakePressed = m_brake || gamepadState.brakePressed;

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
		input.brake = brakePressed;
		m_trackedVehicleTest.SetInput(input);
		return;
	}

	input.throttle = m_moveForward ? 1.0f : (m_moveBackward ? -1.0f : 0.0f);
	input.roll = m_analogRoll != 0.0f
		? m_analogRoll
		: (m_rollLeft ? 1.0f : (m_rollRight ? -1.0f : 0.0f));
	input.brake = brakePressed;

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
		input.throttle == 0.0f &&
		m_trackedVehicleSettings.neutralBrakeEnabled)
	{
		input.brakeAmount =
			std::clamp(m_trackedVehicleSettings.neutralBrakeAmount, 0.0f, 1.0f);
	}

	m_trackedVehicleTest.SetInput(input);
}

void TankSandboxApp::ResetTrackedVehicle()
{
	m_trackedVehicleTest.Initialize(
		m_trackedVehicleSettings,
		m_appliedEnvironmentSettings);
	m_appliedTrackedVehicleSettings = m_trackedVehicleSettings;
	m_trackedVehicleSingleStep = false;
	m_cameraController.ResetFollowState();
	m_trackedVehiclePresenter.UpdateScene(
		m_trackedVehicleTest.State(),
		m_trackedVehicleSettings,
		m_tankVisualSettings,
		m_trackShoeDisplay,
		m_showTrackProxies,
		m_physicsDebugOverlay);
	m_sceneRenderer.SetScene(m_trackedVehiclePresenter.GetScene());
}

void TankSandboxApp::ApplyTrackedVehicleMaterials()
{
	m_trackedVehiclePresenter.ApplyMaterials(m_tankVisualSettings);
	m_sceneRenderer.ReloadSceneResources(m_trackedVehiclePresenter.GetScene());
}

bool TankSandboxApp::SaveTankVisualSettings()
{
	Tank::App::TankVisualSettingsStore store;
	return store.Write(m_tankVisualSettings, m_tankVisualSettingsStatus);
}

bool TankSandboxApp::LoadTankVisualSettings(bool apply)
{
	Tank::App::TankVisualSettingsStore store;
	Tank::Rendering::TankVisualSettings loaded = m_tankVisualSettings;
	if (!store.Read(loaded, m_tankVisualSettingsStatus))
	{
		return false;
	}
	m_tankVisualSettings = loaded;
	if (apply)
	{
		ApplyTrackedVehicleMaterials();
	}
	return true;
}

bool TankSandboxApp::SaveTankSettings()
{
	Tank::App::TankSettingsStore store(m_tankSettingsSlot);
	return store.Write(m_trackedVehicleSettings, m_tankSettingsStatus);
}

bool TankSandboxApp::LoadTankSettings(bool apply)
{
	Tank::App::TankSettingsStore store(m_tankSettingsSlot);
	Tank::Physics::TankSettings loaded = m_trackedVehicleSettings;
	if (!store.Read(loaded, m_tankSettingsStatus))
	{
		return false;
	}
	m_trackedVehicleSettings = loaded;
	if (apply)
	{
		ResetTrackedVehicle();
	}
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
	m_trackedVehiclePresenter.BuildScene(
		m_environmentSettings,
		m_tankVisualSettings,
		m_trackedVehicleSettings);
	Engine::Scene& scene = m_trackedVehiclePresenter.GetScene();
	ActivateOrbitCamera(scene, { 0.0f, 0.8f, 0.0f });

	m_trackedVehicleTest.Initialize(m_trackedVehicleSettings, m_environmentSettings);
	m_appliedTrackedVehicleSettings = m_trackedVehicleSettings;
	m_appliedEnvironmentSettings = m_environmentSettings;
	m_trackedVehiclePresenter.UpdateScene(
		m_trackedVehicleTest.State(),
		m_trackedVehicleSettings,
		m_tankVisualSettings,
		m_trackShoeDisplay,
		m_showTrackProxies,
		m_physicsDebugOverlay);
	m_trackedVehiclePaused = false;
	m_trackedVehicleSingleStep = false;
	m_sceneRenderer.SetScene(scene);
	m_sceneRenderer.ReloadSceneResources(scene);
	m_sceneRenderer.SetDisplayInstanceCount(static_cast<int>(scene.instances.size()));
	m_appMode = AppMode::PhysicsTrackedVehicle;
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
		return &m_boxDropPresenter.GetScene().camera;
	case AppMode::PhysicsTrackedVehicle:
		return &m_trackedVehiclePresenter.GetScene().camera;
	case AppMode::TopMenu:
		return nullptr;
	}

	return nullptr;
}
