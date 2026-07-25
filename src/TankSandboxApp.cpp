#include "stdafx.h"
#include "TankSandboxApp.h"
#include "Platform/Win32Application.h"
#include "Scene/SceneBuilder.h"
#include "imgui.h"

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>

#include <DirectXMath.h>
#include <corecrt.h>
#include <combaseapi.h>
#include <DirectXMathMatrix.inl>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <Engine/Rhi/Dx12/GraphicsDevice.h>
#include <Engine/RtPbrSurveyEngine.h>
#include <Platform/CommandLineOptions.h>
#include <Platform/WindowInfo.h>
#include <Scene/Scene.h>
#include <Shared/Error.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathVector.inl>
#include <system_error>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <Camera/DebugCameraController.h>
#include <GltfLoader.h>
#include <Runtime/SceneRendererDebugUi.h>
#include <Runtime/SceneRendererSettings.h>
#include "Physics/BoxDropTest.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"

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
	// Single large cube right in front of the camera, bright red.
	uint32_t matRed = builder.AddSolidColorMaterial(255, 0, 0, 255);
	builder.AppendCube(1.0f, matRed);
	builder.AddInstance(XMMatrixTranslation(0.0f, 0.0f, 0.0f), matRed);

	// Camera looking at the cube from close range.
	Engine::CameraState camera;
	camera.pos = { 0.0f, 0.0f, -3.0f };
	camera.gazePoint = { 0.0f, 0.0f, 0.0f };
	camera.fov = 60.0f;
	camera.nearZ = 0.001f;
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

	m_defaultRendererSettings = m_sceneRenderer.CaptureSettings();
	LoadRendererSettings();
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
	if (key == VK_ESCAPE)
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
	else if (key == VK_SPACE) m_brake = true;
}

void TankSandboxApp::OnKeyUp(UINT8 key)
{
	if (key == 'W') m_moveForward = false;
	else if (key == 'S') m_moveBackward = false;
	else if (key == 'A') m_turnRight = false;
	else if (key == 'D') m_turnLeft = false;
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

	ImGui::Begin("Camera");
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
	ImGui::End();
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
	camera.nearZ = 0.001f;
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
	ImGui::Begin("Tracked Vehicle");
	ImGui::Text("Step: %d", state.stepIndex);
	ImGui::Text("Time: %.2f s", state.timeSeconds);
	ImGui::Text("Position: %.2f, %.2f, %.2f",
		state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
	ImGui::Text("Sleeping: %s", state.sleeping ? "yes" : "no");
	ImGui::Text("Controls: W/S drive, A/D steer or pivot, Space brake");
	ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
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
	ImGui::Separator();
	ImGui::Text("Press ESC to return to the top menu.");
	ImGui::End();
}

void TankSandboxApp::UpdateTrackedVehicleInput()
{
	Tank::Physics::TankInput input;
	input.throttle = m_moveForward ? 1.0f : (m_moveBackward ? -1.0f : 0.0f);
	input.brake = m_brake;

	if (m_turnLeft != m_turnRight)
	{
		if (input.throttle == 0.0f)
		{
			input.throttle = 1.0f;
			input.leftTrack = m_turnLeft ? -1.0f : 1.0f;
			input.rightTrack = m_turnLeft ? 1.0f : -1.0f;
		}
		else
		{
			input.leftTrack = m_turnLeft ? 0.6f : 1.0f;
			input.rightTrack = m_turnLeft ? 1.0f : 0.6f;
		}
	}

	m_trackedVehicleTest.SetInput(input);
}

void TankSandboxApp::ResetTrackedVehicle()
{
	m_trackedVehicleTest.Initialize();
	m_trackedVehicleSingleStep = false;
	UpdateTrackedVehicleScene(m_trackedVehicleTest.State());
}

void TankSandboxApp::EnterTrackedVehicleMode()
{
	m_trackedVehicleSceneBuilder.Clear();

	const uint32_t floorMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(80, 80, 80, 255);
	const uint32_t bodyMaterial = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(55, 95, 65, 255);
	m_trackedVehicleSceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(40.0f, 0.2f, 40.0f) * XMMatrixTranslation(0.0f, -0.1f, 0.0f),
		floorMaterial);
	m_trackedVehicleBodyInstanceIndex = 1;
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(2.0f, 1.0f, 4.0f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f),
		bodyMaterial);

	Engine::CameraState camera;
	camera.pos = { 8.0f, 5.0f, -12.0f };
	camera.gazePoint = { 0.0f, 1.0f, 0.0f };
	camera.fov = 60.0f;
	camera.nearZ = 0.001f;
	camera.farZ = 10000.0f;
	m_trackedVehicleSceneBuilder.SetCamera(camera);
	ActivateOrbitCamera(m_trackedVehicleSceneBuilder.GetScene(), { 0.0f, 1.0f, 0.0f });

	m_trackedVehicleTest.Initialize();
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
	Engine::InstanceData& body = scene.instances[m_trackedVehicleBodyInstanceIndex];
	body.prevWorld = body.world;
	const XMVECTOR rotation = XMVectorSet(
		state.bodyRotation.x, state.bodyRotation.y, state.bodyRotation.z, state.bodyRotation.w);
	const XMMATRIX world =
		XMMatrixScaling(2.0f, 1.0f, 4.0f) *
		XMMatrixRotationQuaternion(rotation) *
		XMMatrixTranslation(state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
	XMStoreFloat4x4(&body.world, XMMatrixTranspose(world));

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
