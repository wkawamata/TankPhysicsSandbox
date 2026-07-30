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
	constexpr const char* kLegacyTankSettingsPath = "Config/tank_physics.json";
	constexpr const char* kEnvironmentSettingsPath = "Config/physics_environment.json";
	constexpr const char* kTankVisualSettingsPath = "Config/tank_visual.json";

	std::filesystem::path TankSettingsPath(int slot)
	{
		return std::filesystem::path("Config") /
			("tank_physics_slot" + std::to_string(slot + 1) + ".json");
	}

	std::filesystem::path CameraSettingsPath(int slot)
	{
		if (slot == 3)
		{
			return std::filesystem::path("Config") / "camera_debug.json";
		}
		return std::filesystem::path("Config") /
			("camera_slot" + std::to_string(slot + 1) + ".json");
	}

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

	XMMATRIX MakeLineTransform(
		const Tank::Physics::Vec3& start,
		const Tank::Physics::Vec3& end,
		float thickness)
	{
		const XMVECTOR startVector = XMVectorSet(start.x, start.y, start.z, 1.0f);
		const XMVECTOR endVector = XMVectorSet(end.x, end.y, end.z, 1.0f);
		const XMVECTOR delta = XMVectorSubtract(endVector, startVector);
		const float length = XMVectorGetX(XMVector3Length(delta));
		if (length <= 0.0001f)
		{
			return XMMatrixScaling(0.0f, 0.0f, 0.0f);
		}

		const XMVECTOR forward = XMVectorScale(delta, 1.0f / length);
		const float forwardY = std::abs(XMVectorGetY(forward));
		const XMVECTOR referenceUp =
			forwardY < 0.99f
			? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
			: XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		const XMVECTOR right = XMVector3Normalize(XMVector3Cross(referenceUp, forward));
		const XMVECTOR up = XMVector3Cross(forward, right);
		const XMVECTOR midpoint = XMVectorScale(XMVectorAdd(startVector, endVector), 0.5f);

		XMFLOAT3 rightValues;
		XMFLOAT3 upValues;
		XMFLOAT3 forwardValues;
		XMFLOAT3 midpointValues;
		XMStoreFloat3(&rightValues, right);
		XMStoreFloat3(&upValues, up);
		XMStoreFloat3(&forwardValues, forward);
		XMStoreFloat3(&midpointValues, midpoint);
		const XMMATRIX orientation = XMMatrixSet(
			rightValues.x, rightValues.y, rightValues.z, 0.0f,
			upValues.x, upValues.y, upValues.z, 0.0f,
			forwardValues.x, forwardValues.y, forwardValues.z, 0.0f,
			midpointValues.x, midpointValues.y, midpointValues.z, 1.0f);
		return XMMatrixScaling(thickness, thickness, length) * orientation;
	}

	void SetInstanceWorld(Engine::InstanceData& instance, FXMMATRIX world)
	{
		instance.prevWorld = instance.world;
		XMStoreFloat4x4(&instance.world, XMMatrixTranspose(world));
	}

	struct TrackShoePose
	{
		float y = 0.0f;
		float z = 0.0f;
		float tangentY = 0.0f;
		float tangentZ = 1.0f;
	};

	struct TrackPathPoint
	{
		float y = 0.0f;
		float z = 0.0f;
		float tangentY = 0.0f;
		float tangentZ = 1.0f;
		float distanceFromStart = 0.0f;
	};

	struct TrackPathCandidate
	{
		float y = 0.0f;
		float z = 0.0f;
	};

	float TrackPathCross(
		const TrackPathCandidate& origin,
		const TrackPathCandidate& a,
		const TrackPathCandidate& b)
	{
		return
			(a.z - origin.z) * (b.y - origin.y) -
			(a.y - origin.y) * (b.z - origin.z);
	}

	std::vector<TrackPathPoint> BuildTrackPathFromCandidates(
		std::vector<TrackPathCandidate> candidates)
	{
		std::sort(
			candidates.begin(),
			candidates.end(),
			[](const TrackPathCandidate& a, const TrackPathCandidate& b)
			{
				return a.z != b.z ? a.z < b.z : a.y < b.y;
			});

		std::vector<TrackPathCandidate> hull;
		hull.reserve(candidates.size() + 1);
		for (const TrackPathCandidate& candidate : candidates)
		{
			while (hull.size() >= 2 &&
				TrackPathCross(hull[hull.size() - 2], hull.back(), candidate) <= 0.0f)
			{
				hull.pop_back();
			}
			hull.push_back(candidate);
		}

		const size_t lowerHullSize = hull.size();
		for (auto candidate = candidates.rbegin() + 1; candidate != candidates.rend(); ++candidate)
		{
			while (hull.size() > lowerHullSize &&
				TrackPathCross(hull[hull.size() - 2], hull.back(), *candidate) <= 0.0f)
			{
				hull.pop_back();
			}
			hull.push_back(*candidate);
		}

		if (hull.size() < 4)
		{
			return {};
		}
		hull.back() = hull.front();

		std::vector<TrackPathPoint> path;
		path.reserve(hull.size());
		for (const TrackPathCandidate& candidate : hull)
		{
			TrackPathPoint point;
			point.y = candidate.y;
			point.z = candidate.z;
			if (!path.empty())
			{
				const TrackPathPoint& previous = path.back();
				const float deltaY = point.y - previous.y;
				const float deltaZ = point.z - previous.z;
				const float segmentLength = std::sqrt(deltaY * deltaY + deltaZ * deltaZ);
				point.distanceFromStart = previous.distanceFromStart + segmentLength;
				if (segmentLength > 0.0f)
				{
					path.back().tangentY = deltaY / segmentLength;
					path.back().tangentZ = deltaZ / segmentLength;
				}
			}
			path.push_back(point);
		}
		path.back().tangentY = path.front().tangentY;
		path.back().tangentZ = path.front().tangentZ;
		return path;
	}

	std::vector<TrackPathPoint> BuildTrackPath(float chassisLength, float radius)
	{
		constexpr int kArcSegments = 8;
		const float halfStraight = (std::max)(0.1f, 0.5f * chassisLength - radius);
		std::vector<TrackPathPoint> path;
		path.reserve(2 * kArcSegments + 4);

		auto appendPoint = [&path](float y, float z)
		{
			TrackPathPoint point;
			point.y = y;
			point.z = z;
			if (!path.empty())
			{
				const TrackPathPoint& previous = path.back();
				const float deltaY = y - previous.y;
				const float deltaZ = z - previous.z;
				const float segmentLength = std::sqrt(deltaY * deltaY + deltaZ * deltaZ);
				point.distanceFromStart = previous.distanceFromStart + segmentLength;
				if (segmentLength > 0.0f)
				{
					path.back().tangentY = deltaY / segmentLength;
					path.back().tangentZ = deltaZ / segmentLength;
				}
			}
			path.push_back(point);
		};

		appendPoint(-radius, -halfStraight);
		appendPoint(-radius, halfStraight);
		for (int segment = 1; segment <= kArcSegments; ++segment)
		{
			const float angle =
				-0.5f * XM_PI +
				XM_PI * static_cast<float>(segment) / static_cast<float>(kArcSegments);
			appendPoint(
				radius * std::sin(angle),
				halfStraight + radius * std::cos(angle));
		}
		appendPoint(radius, -halfStraight);
		for (int segment = 1; segment <= kArcSegments; ++segment)
		{
			const float angle =
				0.5f * XM_PI +
				XM_PI * static_cast<float>(segment) / static_cast<float>(kArcSegments);
			appendPoint(
				radius * std::sin(angle),
				-halfStraight + radius * std::cos(angle));
		}

		if (path.size() >= 2)
		{
			path.back().tangentY = path.front().tangentY;
			path.back().tangentZ = path.front().tangentZ;
		}
		return path;
	}

	std::vector<TrackPathPoint> BuildTrackPathFromWheels(
		const Tank::Physics::TrackedVehicleTestState& state,
		const Tank::Physics::TankSettings& settings,
		int trackIndex,
		DirectX::FXMMATRIX inverseBodyTransform)
	{
		constexpr int kWheelPathSegments = 32;
		constexpr float kTrackClearanceM = 0.08f;
		std::vector<TrackPathCandidate> candidates;
		candidates.reserve(
			static_cast<size_t>(state.wheelCount) *
			static_cast<size_t>(kWheelPathSegments));

		const int wheelsPerSurface = settings.roadWheelCount + 2;
		for (int wheelIndex = 0; wheelIndex < state.wheelCount; ++wheelIndex)
		{
			const Tank::Physics::TrackedWheelState& wheel =
				state.wheels[static_cast<size_t>(wheelIndex)];
			if (wheel.trackIndex != trackIndex)
			{
				continue;
			}

			const XMVECTOR worldCenter = XMVectorSet(
				wheel.transform.position.x,
				wheel.transform.position.y,
				wheel.transform.position.z,
				1.0f);
			const XMVECTOR localCenter =
				XMVector3TransformCoord(worldCenter, inverseBodyTransform);
			const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
			const bool endWheel =
				wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
			const float wheelRadius = endWheel
				? settings.endWheelRadiusM
				: settings.roadWheelRadiusM;
			const float pathRadius =
				(wheelRadius + kTrackClearanceM) /
				std::cos(XM_PI / static_cast<float>(kWheelPathSegments));

			for (int segment = 0; segment < kWheelPathSegments; ++segment)
			{
				const float angle =
					2.0f * XM_PI * static_cast<float>(segment) /
					static_cast<float>(kWheelPathSegments);
				candidates.push_back({
					XMVectorGetY(localCenter) + pathRadius * std::sin(angle),
					XMVectorGetZ(localCenter) + pathRadius * std::cos(angle) });
			}
		}

		if (candidates.empty())
		{
			return {};
		}
		return BuildTrackPathFromCandidates(std::move(candidates));
	}

	TrackShoePose CalculateTrackShoePose(
		const std::vector<TrackPathPoint>& path,
		float distance)
	{
		if (path.size() < 2 || path.back().distanceFromStart <= 0.0f)
		{
			return {};
		}

		const float pathLength = path.back().distanceFromStart;
		distance = std::fmod(distance, pathLength);
		if (distance < 0.0f)
		{
			distance += pathLength;
		}

		const auto end = std::upper_bound(
			path.begin(),
			path.end(),
			distance,
			[](float value, const TrackPathPoint& point)
			{
				return value < point.distanceFromStart;
			});
		const size_t endIndex = static_cast<size_t>(std::distance(path.begin(), end));
		const size_t nextIndex = (std::max)(size_t{ 1 }, endIndex);
		const TrackPathPoint& start = path[nextIndex - 1];
		const TrackPathPoint& next = path[nextIndex];
		const float segmentLength = next.distanceFromStart - start.distanceFromStart;
		const float t = segmentLength > 0.0f
			? (distance - start.distanceFromStart) / segmentLength
			: 0.0f;
		const float tangentY = start.tangentY + (next.tangentY - start.tangentY) * t;
		const float tangentZ = start.tangentZ + (next.tangentZ - start.tangentZ) * t;
		const float tangentLength =
			std::sqrt(tangentY * tangentY + tangentZ * tangentZ);

		return {
			start.y + (next.y - start.y) * t,
			start.z + (next.z - start.z) * t,
			tangentLength > 0.0f ? tangentY / tangentLength : 0.0f,
			tangentLength > 0.0f ? tangentZ / tangentLength : 1.0f };
	}

	XMMATRIX MakeTrackShoeLocalTransform(
		float x,
		const TrackShoePose& pose,
		float width,
		float thickness,
		float length)
	{
		const XMMATRIX orientation = XMMatrixSet(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, pose.tangentZ, -pose.tangentY, 0.0f,
			0.0f, pose.tangentY, pose.tangentZ, 0.0f,
			x, pose.y, pose.z, 1.0f);
		return XMMatrixScaling(width, thickness, length) * orientation;
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
	if (m_appMode != AppMode::TopMenu &&
		!(m_appMode == AppMode::PhysicsTrackedVehicle && m_trackedVehicleCameraFollow))
	{
		m_debugCameraController.OnMouseDown(button, x, y);
	}
}

void TankSandboxApp::OnMouseUp(UINT8 button, int x, int y)
{
	if (m_appMode != AppMode::TopMenu &&
		!(m_appMode == AppMode::PhysicsTrackedVehicle && m_trackedVehicleCameraFollow))
	{
		m_debugCameraController.OnMouseUp(button, x, y);
		ApplyActiveCameraScene();
	}
}

void TankSandboxApp::OnMouseMove(int x, int y)
{
	if (m_appMode != AppMode::TopMenu &&
		!(m_appMode == AppMode::PhysicsTrackedVehicle && m_trackedVehicleCameraFollow))
	{
		m_debugCameraController.OnMouseMove(x, y);
		ApplyActiveCameraScene();
	}
}

void TankSandboxApp::OnMouseWheel(int wheelDelta)
{
	if (m_appMode != AppMode::TopMenu &&
		!(m_appMode == AppMode::PhysicsTrackedVehicle && m_trackedVehicleCameraFollow))
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
		const Tank::Input::GamepadState& cameraGamepadState = m_gamepad.State();
		const bool cameraButton4Pressed =
			cameraGamepadState.connected &&
			cameraGamepadState.buttonCount > 4 &&
			cameraGamepadState.rawButtons[4];
		const bool cameraButton7Pressed =
			cameraGamepadState.connected &&
			cameraGamepadState.buttonCount > 7 &&
			cameraGamepadState.rawButtons[7];
		if (cameraButton4Pressed && !m_cameraButton4WasPressed)
		{
			SelectCameraSlot((m_cameraSettingsSlot + 1) % 3, true);
		}
		else if (cameraButton7Pressed && !m_cameraButton7WasPressed)
		{
			SelectCameraSlot((m_cameraSettingsSlot + 2) % 3, true);
		}
		m_cameraButton4WasPressed = cameraButton4Pressed;
		m_cameraButton7WasPressed = cameraButton7Pressed;
		UpdateTrackedVehicleInput();
		if (!m_trackedVehiclePaused || m_trackedVehicleSingleStep)
		{
			const Tank::Physics::TrackedVehicleTestState state = m_trackedVehicleTest.Step(kPhysicsFixedDt);
			UpdateTrackedVehicleScene(state);
			m_trackedVehicleSingleStep = false;
		}
		UpdateTrackedVehicleFollowCamera(
			m_trackedVehicleTest.State(),
			kPhysicsFixedDt);
	}
	UpdateCameraTransition(kPhysicsFixedDt);
	UpdateCameraSlotCache();

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
			m_cameraSettingsSlot == slot))
		{
			SelectCameraSlot(slot, m_cameraSettingsAutoLoad);
		}
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Debug", m_cameraSettingsSlot == 3))
	{
		SelectCameraSlot(3, m_cameraSettingsAutoLoad);
	}
	ImGui::SameLine();
	ImGui::Checkbox("AutoLoad", &m_cameraSettingsAutoLoad);
	if (ImGui::Button("Save Camera"))
	{
		SaveCameraSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Camera"))
	{
		LoadCameraSettings();
	}
	if (!m_cameraSettingsStatus.empty())
	{
		ImGui::TextWrapped("%s", m_cameraSettingsStatus.c_str());
	}
	if (m_appMode == AppMode::PhysicsTrackedVehicle)
	{
		if (ImGui::Checkbox("Follow Tank", &m_trackedVehicleCameraFollow))
		{
			m_trackedVehicleCameraVelocity = {};
			m_trackedVehicleCameraYawVelocity = 0.0f;
			m_trackedVehicleCameraOrbitInitialized = false;
			if (!m_trackedVehicleCameraFollow)
			{
				const Tank::Physics::TrackedVehicleTestState& state =
					m_trackedVehicleTest.State();
				ActivateOrbitCamera(
					m_trackedVehicleSceneBuilder.GetScene(),
					{
						state.bodyPosition.x,
						state.bodyPosition.y + 0.5f,
						state.bodyPosition.z });
			}
		}
		ImGui::BeginDisabled(!m_trackedVehicleCameraFollow);
		ImGuiWidgets::SliderFloatWithControls(
			"Follow Distance",
			&m_trackedVehicleCameraFollowDistance,
			4.0f,
			250.0f,
			0.5f,
			16.0f);
		ImGuiWidgets::SliderFloatWithControls(
			"Look Down Angle",
			&m_trackedVehicleCameraLookDownDegrees,
			0.0f,
			89.0f,
			1.0f,
			25.0f,
			"%.1f deg");
		ImGuiWidgets::SliderFloatWithControls(
			"Position Speed",
			&m_trackedVehicleCameraPositionSpeed,
			0.5f,
			20.0f,
			0.5f,
			5.0f);
		ImGuiWidgets::SliderFloatWithControls(
			"Rotation Speed",
			&m_trackedVehicleCameraRotationSpeed,
			0.5f,
			20.0f,
			0.5f,
			8.0f);
		ImGuiWidgets::SliderFloatWithControls(
			"Yaw Speed Limit",
			&m_trackedVehicleCameraYawSpeedLimitDegrees,
			15.0f,
			720.0f,
			15.0f,
			180.0f,
			"%.0f deg/s");
		ImGuiWidgets::SliderFloatWithControls(
			"Yaw Damping",
			&m_trackedVehicleCameraYawDamping,
			0.5f,
			30.0f,
			0.5f,
			8.0f);
		ImGuiWidgets::SliderFloatWithControls(
			"Damping",
			&m_trackedVehicleCameraDamping,
			0.1f,
			2.0f,
			0.05f,
			1.0f);
		ImGui::EndDisabled();
		ImGui::BeginDisabled(m_trackedVehicleCameraFollow);
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
			m_trackedVehicleCameraFovTarget = camera->fov;
			m_trackedVehicleCameraFovVelocity = 0.0f;
			m_trackedVehicleCameraFovSpringActive = false;
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
	if (ActiveCamera() == nullptr)
	{
		m_cameraSettingsStatus = "Save failed: no active camera";
		return false;
	}
	const Tank::Rendering::CameraSettings settings = CaptureCameraSettings();

	const std::filesystem::path path = CameraSettingsPath(m_cameraSettingsSlot);
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		m_cameraSettingsStatus = "Save failed: cannot write file";
		return false;
	}
	output << Tank::Rendering::SerializeCameraSettings(settings);
	if (!output)
	{
		m_cameraSettingsStatus = "Save failed: write error";
		return false;
	}
	m_cameraSettingsCache[static_cast<size_t>(m_cameraSettingsSlot)] = settings;
	m_cameraSettingsDirty[static_cast<size_t>(m_cameraSettingsSlot)] = false;
	m_cameraSettingsFileLoaded[static_cast<size_t>(m_cameraSettingsSlot)] = true;
	m_cameraSettingsStatus = "Saved: " + path.string();
	return true;
}

bool TankSandboxApp::LoadCameraSettings()
{
	if (!EnsureCameraSlotLoaded(m_cameraSettingsSlot))
	{
		return false;
	}
	ApplyCameraSettings(
		*m_cameraSettingsCache[static_cast<size_t>(m_cameraSettingsSlot)],
		true);
	m_cameraSettingsStatus = m_cameraSettingsSlot == 3
		? "Loaded debug camera"
		: "Loaded slot " + std::to_string(m_cameraSettingsSlot + 1);
	return true;
}

Tank::Rendering::CameraSettings TankSandboxApp::CaptureCameraSettings()
{
	Tank::Rendering::CameraSettings settings;
	const Engine::CameraState* camera = ActiveCamera();
	if (camera == nullptr)
	{
		return settings;
	}
	settings.position[0] = camera->pos.x;
	settings.position[1] = camera->pos.y;
	settings.position[2] = camera->pos.z;
	settings.gazePoint[0] = camera->gazePoint.x;
	settings.gazePoint[1] = camera->gazePoint.y;
	settings.gazePoint[2] = camera->gazePoint.z;
	settings.projection = static_cast<int>(camera->projection);
	settings.fovDegrees = camera->fov;
	settings.orthographicHeight = camera->orthographicHeight;
	settings.followTank = m_trackedVehicleCameraFollow;
	settings.followDistance = m_trackedVehicleCameraFollowDistance;
	settings.lookDownDegrees = m_trackedVehicleCameraLookDownDegrees;
	settings.positionSpeed = m_trackedVehicleCameraPositionSpeed;
	settings.rotationSpeed = m_trackedVehicleCameraRotationSpeed;
	settings.damping = m_trackedVehicleCameraDamping;
	settings.yawSpeedLimitDegrees = m_trackedVehicleCameraYawSpeedLimitDegrees;
	settings.yawDamping = m_trackedVehicleCameraYawDamping;
	return settings;
}

bool TankSandboxApp::EnsureCameraSlotLoaded(int slot)
{
	const size_t slotIndex = static_cast<size_t>(std::clamp(slot, 0, 3));
	if (m_cameraSettingsFileLoaded[slotIndex] &&
		m_cameraSettingsCache[slotIndex].has_value())
	{
		return true;
	}
	const std::filesystem::path path = CameraSettingsPath(static_cast<int>(slotIndex));
	std::ifstream input(path, std::ios::binary);
	if (!input)
	{
		m_cameraSettingsStatus = "No saved camera in slot";
		return false;
	}
	const std::string json(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
	Tank::Rendering::CameraSettings settings;
	std::string error;
	if (!Tank::Rendering::DeserializeCameraSettings(json, settings, &error))
	{
		m_cameraSettingsStatus = "Load failed: " + error;
		return false;
	}
	m_cameraSettingsCache[slotIndex] = settings;
	m_cameraSettingsDirty[slotIndex] = false;
	m_cameraSettingsFileLoaded[slotIndex] = true;
	return true;
}

void TankSandboxApp::ApplyCameraSettings(
	const Tank::Rendering::CameraSettings& settings,
	bool smooth)
{
	if (ActiveCamera() == nullptr)
	{
		return;
	}
	if (smooth && settings.followTank)
	{
		Engine::CameraState* camera = ActiveCamera();
		const XMFLOAT3 currentPosition = camera->pos;
		const XMFLOAT3 currentGazePoint = camera->gazePoint;
		const float currentFov = camera->fov;
		ApplyCameraSettings(settings, false);
		camera->pos = currentPosition;
		camera->gazePoint = currentGazePoint;
		camera->fov = currentFov;
		m_trackedVehicleCameraFovTarget =
			std::clamp(settings.fovDegrees, 20.0f, 120.0f);
		m_trackedVehicleCameraFovVelocity = 0.0f;
		m_trackedVehicleCameraFovSpringActive =
			std::abs(camera->fov - m_trackedVehicleCameraFovTarget) > 0.001f;
		m_cameraTransitionActive = false;
		m_sceneRenderer.SetCamera(*camera);
		return;
	}
	if (smooth)
	{
		m_cameraTransitionStart = CaptureCameraSettings();
		m_cameraTransitionTarget = settings;
		m_cameraTransitionTime = 0.0f;
		m_cameraTransitionActive = true;
		m_trackedVehicleCameraFollow = false;
		return;
	}

	Engine::CameraState* camera = ActiveCamera();
	camera->pos = {
		settings.position[0],
		settings.position[1],
		settings.position[2] };
	camera->gazePoint = {
		settings.gazePoint[0],
		settings.gazePoint[1],
		settings.gazePoint[2] };
	camera->projection =
		settings.projection == static_cast<int>(Engine::CameraProjection::Orthographic)
		? Engine::CameraProjection::Orthographic
		: Engine::CameraProjection::Perspective;
	camera->fov = std::clamp(settings.fovDegrees, 20.0f, 120.0f);
	m_trackedVehicleCameraFovTarget = camera->fov;
	m_trackedVehicleCameraFovVelocity = 0.0f;
	m_trackedVehicleCameraFovSpringActive = false;
	camera->orthographicHeight =
		std::clamp(settings.orthographicHeight, 1.0f, 50.0f);
	m_trackedVehicleCameraFollow = settings.followTank;
	m_trackedVehicleCameraFollowDistance =
		std::clamp(settings.followDistance, 4.0f, 250.0f);
	m_trackedVehicleCameraLookDownDegrees =
		std::clamp(settings.lookDownDegrees, 0.0f, 89.0f);
	m_trackedVehicleCameraPositionSpeed =
		std::clamp(settings.positionSpeed, 0.5f, 20.0f);
	m_trackedVehicleCameraRotationSpeed =
		std::clamp(settings.rotationSpeed, 0.5f, 20.0f);
	m_trackedVehicleCameraDamping =
		std::clamp(settings.damping, 0.1f, 2.0f);
	m_trackedVehicleCameraYawSpeedLimitDegrees =
		std::clamp(settings.yawSpeedLimitDegrees, 15.0f, 720.0f);
	m_trackedVehicleCameraYawDamping =
		std::clamp(settings.yawDamping, 0.5f, 30.0f);
	m_trackedVehicleCameraVelocity = {};
	m_trackedVehicleCameraFovVelocity = 0.0f;
	m_trackedVehicleCameraFovSpringActive = false;
	m_trackedVehicleCameraYawVelocity = 0.0f;
	m_trackedVehicleCameraOrbitInitialized = false;
	m_sceneRenderer.SetCamera(*camera);
}

void TankSandboxApp::SelectCameraSlot(int slot, bool load)
{
	const size_t currentSlot = static_cast<size_t>(m_cameraSettingsSlot);
	if (!m_cameraSettingsCache[currentSlot].has_value() &&
		ActiveCamera() != nullptr)
	{
		m_cameraSettingsCache[currentSlot] = CaptureCameraSettings();
		m_cameraSettingsDirty[currentSlot] = true;
	}
	else
	{
		UpdateCameraSlotCache();
	}
	m_cameraSettingsSlot = std::clamp(slot, 0, 3);
	if (load)
	{
		LoadCameraSettings();
	}
}

void TankSandboxApp::UpdateCameraTransition(float deltaTimeSeconds)
{
	if (!m_cameraTransitionActive || ActiveCamera() == nullptr)
	{
		return;
	}
	m_cameraTransitionTime += std::max(deltaTimeSeconds, 0.0f);
	const float normalizedTime = std::clamp(
		m_cameraTransitionTime / m_cameraTransitionDuration,
		0.0f,
		1.0f);
	const float t = normalizedTime * normalizedTime * (3.0f - 2.0f * normalizedTime);
	Tank::Rendering::CameraSettings blended = m_cameraTransitionStart;
	for (int axis = 0; axis < 3; ++axis)
	{
		blended.position[axis] =
			std::lerp(
				m_cameraTransitionStart.position[axis],
				m_cameraTransitionTarget.position[axis],
				t);
		blended.gazePoint[axis] =
			std::lerp(
				m_cameraTransitionStart.gazePoint[axis],
				m_cameraTransitionTarget.gazePoint[axis],
				t);
	}
	blended.fovDegrees =
		std::lerp(
			m_cameraTransitionStart.fovDegrees,
			m_cameraTransitionTarget.fovDegrees,
			t);
	blended.orthographicHeight =
		std::lerp(
			m_cameraTransitionStart.orthographicHeight,
			m_cameraTransitionTarget.orthographicHeight,
			t);
	blended.followDistance =
		std::lerp(
			m_cameraTransitionStart.followDistance,
			m_cameraTransitionTarget.followDistance,
			t);
	blended.lookDownDegrees =
		std::lerp(
			m_cameraTransitionStart.lookDownDegrees,
			m_cameraTransitionTarget.lookDownDegrees,
			t);
	blended.followTank = false;
	blended.projection = normalizedTime < 0.5f
		? m_cameraTransitionStart.projection
		: m_cameraTransitionTarget.projection;
	ApplyCameraSettings(blended, false);
	if (normalizedTime >= 1.0f)
	{
		m_cameraTransitionActive = false;
		ApplyCameraSettings(m_cameraTransitionTarget, false);
	}
}

void TankSandboxApp::UpdateCameraSlotCache()
{
	if (m_cameraTransitionActive || ActiveCamera() == nullptr)
	{
		return;
	}
	const size_t slot = static_cast<size_t>(m_cameraSettingsSlot);
	if (!m_cameraSettingsCache[slot].has_value())
	{
		return;
	}
	Tank::Rendering::CameraSettings current = CaptureCameraSettings();
	if (current.followTank && m_cameraSettingsCache[slot]->followTank)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			current.position[axis] = m_cameraSettingsCache[slot]->position[axis];
			current.gazePoint[axis] = m_cameraSettingsCache[slot]->gazePoint[axis];
		}
	}
	if (!m_cameraSettingsDirty[slot] &&
		Tank::Rendering::SerializeCameraSettings(current) !=
			Tank::Rendering::SerializeCameraSettings(*m_cameraSettingsCache[slot]))
	{
		m_cameraSettingsDirty[slot] = true;
	}
	if (m_cameraSettingsDirty[slot])
	{
		m_cameraSettingsCache[slot] = current;
	}
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
	if (ImGui::Checkbox("Physics Debug Overlay", &m_physicsDebugOverlay))
	{
		UpdateTrackedVehicleScene(state);
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
		UpdateTrackedVehicleScene(state);
	}
	if (ImGui::Checkbox("Show Track Proxies", &m_showTrackProxies))
	{
		UpdateTrackedVehicleScene(state);
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
	m_trackedVehicleCameraVelocity = {};
	m_trackedVehicleCameraYawVelocity = 0.0f;
	m_trackedVehicleCameraOrbitInitialized = false;
	m_trackShoeDistances.fill(0.0f);
	m_trackShoeLastTimeSeconds = 0.0f;
	UpdateTrackedVehicleScene(m_trackedVehicleTest.State());
}

void TankSandboxApp::ApplyTrackedVehicleMaterials()
{
	Engine::SceneMesh& mesh = m_trackedVehicleSceneBuilder.GetMesh();
	auto applyColor = [&mesh](
		uint32_t materialId,
		const Tank::Rendering::BodyMaterialSettings& settings)
	{
		if (materialId >= mesh.materials.size())
		{
			return;
		}
		const int textureIndex = mesh.materials[materialId].albedoTexIndex;
		if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= mesh.textures.size())
		{
			return;
		}
		Engine::SceneTexture& texture = mesh.textures[static_cast<size_t>(textureIndex)];
		if (texture.pixels.size() < 4)
		{
			return;
		}
		texture.pixels[0] = static_cast<uint8_t>(
			std::clamp(settings.albedo.r, 0.0f, 1.0f) * 255.0f);
		texture.pixels[1] = static_cast<uint8_t>(
			std::clamp(settings.albedo.g, 0.0f, 1.0f) * 255.0f);
		texture.pixels[2] = static_cast<uint8_t>(
			std::clamp(settings.albedo.b, 0.0f, 1.0f) * 255.0f);
		texture.pixels[3] = 255;
		Engine::SceneMaterial& material = mesh.materials[materialId];
		material.roughnessFactor = std::clamp(settings.roughness, 0.04f, 1.0f);
		material.metallicFactor = std::clamp(settings.metallic, 0.0f, 1.0f);
		material.ambientOcclusionFactor =
			std::clamp(settings.ambientOcclusion, 0.0f, 1.0f);
		material.emissiveScale = std::clamp(settings.emissive, 0.0f, 4.0f);
	};

	applyColor(m_trackedVehicleModel.hullUpperMaterial, m_tankVisualSettings.hullUpper);
	applyColor(m_trackedVehicleModel.hullLowerMaterial, m_tankVisualSettings.hullLower);
	applyColor(
		m_trackedVehicleModel.structureUpperMaterial,
		m_tankVisualSettings.structureUpper);
	applyColor(
		m_trackedVehicleModel.structureLowerMaterial,
		m_tankVisualSettings.structureLower);
	applyColor(m_trackedVehicleModel.wheelMaterial, m_tankVisualSettings.wheels);
	applyColor(
		m_trackedVehicleModel.contactedWheelMaterial,
		m_tankVisualSettings.contactedWheels);
	applyColor(m_trackedVehicleModel.trackShoeMaterial, m_tankVisualSettings.trackShoes);
	applyColor(m_trackedVehicleModel.trackProxyMaterial, m_tankVisualSettings.trackProxies);
	applyColor(
		m_trackedVehicleModel.forwardMarkerMaterial,
		m_tankVisualSettings.forwardMarker);
	m_sceneRenderer.ReloadSceneResources(m_trackedVehicleSceneBuilder.GetScene());
}

bool TankSandboxApp::SaveTankVisualSettings()
{
	const std::filesystem::path path(kTankVisualSettingsPath);
	std::error_code errorCode;
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		m_tankVisualSettingsStatus = "Save failed: " + errorCode.message();
		return false;
	}

	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
	{
		m_tankVisualSettingsStatus = "Save failed: cannot open file";
		return false;
	}
	output << Tank::Rendering::SerializeTankVisualSettings(m_tankVisualSettings);
	if (!output)
	{
		m_tankVisualSettingsStatus = "Save failed: cannot write file";
		return false;
	}
	m_tankVisualSettingsStatus = std::string("Saved: ") + kTankVisualSettingsPath;
	return true;
}

bool TankSandboxApp::LoadTankVisualSettings(bool apply)
{
	std::ifstream input(kTankVisualSettingsPath, std::ios::binary);
	if (!input)
	{
		m_tankVisualSettingsStatus = "Load failed: no saved settings";
		return false;
	}
	const std::string json(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
	Tank::Rendering::TankVisualSettings loaded = m_tankVisualSettings;
	std::string error;
	if (!Tank::Rendering::DeserializeTankVisualSettings(json, loaded, &error))
	{
		m_tankVisualSettingsStatus = "Load failed: " + error;
		return false;
	}
	m_tankVisualSettings = loaded;
	if (apply)
	{
		ApplyTrackedVehicleMaterials();
	}
	m_tankVisualSettingsStatus = std::string("Loaded: ") + kTankVisualSettingsPath;
	return true;
}

bool TankSandboxApp::SaveTankSettings()
{
	const std::filesystem::path path = TankSettingsPath(m_tankSettingsSlot);
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

	m_tankSettingsStatus = "Saved: " + path.string();
	return true;
}

bool TankSandboxApp::LoadTankSettings(bool apply)
{
	std::filesystem::path path = TankSettingsPath(m_tankSettingsSlot);
	std::ifstream input(path, std::ios::binary);
	if (!input && m_tankSettingsSlot == 0)
	{
		path = kLegacyTankSettingsPath;
		input = std::ifstream(path, std::ios::binary);
	}
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
	if (apply)
	{
		ResetTrackedVehicle();
	}
	m_tankSettingsStatus = "Loaded: " + path.string();
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
	auto addBodyMaterial = [this](const Tank::Rendering::BodyMaterialSettings& material)
	{
		const uint32_t materialId = m_trackedVehicleSceneBuilder.AddSolidColorMaterial(
			static_cast<uint8_t>(std::clamp(material.albedo.r, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8_t>(std::clamp(material.albedo.g, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8_t>(std::clamp(material.albedo.b, 0.0f, 1.0f) * 255.0f),
			255);
		Engine::SceneMaterial& sceneMaterial =
			m_trackedVehicleSceneBuilder.GetMesh().materials[materialId];
		sceneMaterial.roughnessFactor = std::clamp(material.roughness, 0.04f, 1.0f);
		sceneMaterial.metallicFactor = std::clamp(material.metallic, 0.0f, 1.0f);
		sceneMaterial.ambientOcclusionFactor =
			std::clamp(material.ambientOcclusion, 0.0f, 1.0f);
		sceneMaterial.emissiveScale = std::clamp(material.emissive, 0.0f, 4.0f);
		return materialId;
	};
	m_trackedVehicleModel.hullUpperMaterial = addBodyMaterial(m_tankVisualSettings.hullUpper);
	m_trackedVehicleModel.hullLowerMaterial = addBodyMaterial(m_tankVisualSettings.hullLower);
	m_trackedVehicleModel.structureUpperMaterial =
		addBodyMaterial(m_tankVisualSettings.structureUpper);
	m_trackedVehicleModel.structureLowerMaterial =
		addBodyMaterial(m_tankVisualSettings.structureLower);
	m_trackedVehicleModel.trackProxyMaterial =
		addBodyMaterial(m_tankVisualSettings.trackProxies);
	m_trackedVehicleModel.forwardMarkerMaterial =
		addBodyMaterial(m_tankVisualSettings.forwardMarker);
	m_trackedVehicleModel.wheelMaterial = addBodyMaterial(m_tankVisualSettings.wheels);
	m_trackedVehicleModel.contactedWheelMaterial =
		addBodyMaterial(m_tankVisualSettings.contactedWheels);
	m_trackedVehicleModel.trackShoeMaterial =
		addBodyMaterial(m_tankVisualSettings.trackShoes);
	const uint32_t obstacleMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(70, 95, 135, 255);
	m_trackedVehicleModel.debugContactMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(60, 230, 90, 255);
	m_trackedVehicleModel.debugAirborneMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(255, 145, 35, 255);
	const uint32_t debugSuspensionMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(40, 210, 230, 255);
	const uint32_t debugNormalMaterial =
		m_trackedVehicleSceneBuilder.AddSolidColorMaterial(255, 225, 45, 255);

	m_trackedVehicleSceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
	const Engine::SceneMeshId wheelMesh = m_trackedVehicleSceneBuilder.AddCylinder(
		1.0f,
		1.0f,
		16,
		Engine::CylinderCapMode::Both);

	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(
			m_environmentSettings.floorSizeM,
			0.2f,
			m_environmentSettings.floorSizeM) *
			XMMatrixTranslation(0.0f, -0.1f, 0.0f),
		floorMaterial);

	m_trackedVehicleModel.hullUpper =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(2.16f, 0.25f, 3.5f) *
			XMMatrixTranslation(0.0f, 2.125f, 0.0f),
		m_trackedVehicleModel.hullUpperMaterial);
	m_trackedVehicleModel.hullLower =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(2.16f, 0.25f, 3.5f) *
			XMMatrixTranslation(0.0f, 1.875f, 0.0f),
		m_trackedVehicleModel.hullLowerMaterial);
	m_trackedVehicleModel.upperStructureUpper =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.125f, 2.0f) *
			XMMatrixTranslation(0.0f, 2.4375f, 0.3f),
		m_trackedVehicleModel.structureUpperMaterial);
	m_trackedVehicleModel.upperStructureLower =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.125f, 2.0f) *
			XMMatrixTranslation(0.0f, 2.3125f, 0.3f),
		m_trackedVehicleModel.structureLowerMaterial);
	m_trackedVehicleModel.lowerStructureUpper =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.125f, 2.0f) *
			XMMatrixTranslation(0.0f, 1.6875f, 0.3f),
		m_trackedVehicleModel.structureUpperMaterial);
	m_trackedVehicleModel.lowerStructureLower =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(1.44f, 0.125f, 2.0f) *
			XMMatrixTranslation(0.0f, 1.5625f, 0.3f),
		m_trackedVehicleModel.structureLowerMaterial);

	m_trackedVehicleModel.leftTrack =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, 4.0f) *
		XMMatrixTranslation(-0.5f * m_trackedVehicleSettings.trackSpacingM, 2.0f, 0.0f),
		m_trackedVehicleModel.trackProxyMaterial);

	m_trackedVehicleModel.rightTrack =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, 4.0f) *
		XMMatrixTranslation(0.5f * m_trackedVehicleSettings.trackSpacingM, 2.0f, 0.0f),
		m_trackedVehicleModel.trackProxyMaterial);

	m_trackedVehicleModel.forwardMarker =
		m_trackedVehicleSceneBuilder.GetScene().instances.size();
	m_trackedVehicleSceneBuilder.AddInstance(
		XMMatrixScaling(0.3f, 0.3f, 0.3f) * XMMatrixTranslation(0.0f, 2.0f, 2.5f),
		m_trackedVehicleModel.forwardMarkerMaterial);

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		m_trackedVehicleModel.wheels[static_cast<size_t>(i)] =
			m_trackedVehicleSceneBuilder.GetScene().instances.size();
		m_trackedVehicleSceneBuilder.AddInstance(
			wheelMesh,
			XMMatrixScaling(0.0f, 0.0f, 0.0f),
			m_trackedVehicleModel.wheelMaterial);
	}

	for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
	{
		for (int shoe = 0; shoe < TrackedVehicleModel::kTrackShoeCountPerTrack; ++shoe)
		{
			m_trackedVehicleModel.trackShoes[static_cast<size_t>(track)]
				[static_cast<size_t>(shoe)] =
				m_trackedVehicleSceneBuilder.GetScene().instances.size();
			m_trackedVehicleSceneBuilder.AddInstance(
				XMMatrixScaling(0.0f, 0.0f, 0.0f),
				m_trackedVehicleModel.trackShoeMaterial);
		}
	}

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		const size_t wheelIndex = static_cast<size_t>(i);
		m_trackedVehicleModel.suspensionLines[wheelIndex] =
			m_trackedVehicleSceneBuilder.GetScene().instances.size();
		m_trackedVehicleSceneBuilder.AddInstance(
			XMMatrixScaling(0.0f, 0.0f, 0.0f),
			debugSuspensionMaterial);
		m_trackedVehicleModel.contactMarkers[wheelIndex] =
			m_trackedVehicleSceneBuilder.GetScene().instances.size();
		m_trackedVehicleSceneBuilder.AddInstance(
			XMMatrixScaling(0.0f, 0.0f, 0.0f),
			m_trackedVehicleModel.debugAirborneMaterial);
		m_trackedVehicleModel.contactNormalLines[wheelIndex] =
			m_trackedVehicleSceneBuilder.GetScene().instances.size();
		m_trackedVehicleSceneBuilder.AddInstance(
			XMMatrixScaling(0.0f, 0.0f, 0.0f),
			debugNormalMaterial);
	}

	for (const Tank::Physics::TestObstaclePlacement& obstacle :
		Tank::Physics::GenerateTestObstacleLayout(m_environmentSettings))
	{
		m_trackedVehicleSceneBuilder.AddInstance(
			XMMatrixScaling(
				Tank::Physics::kPassengerCarWidthM,
				Tank::Physics::kPassengerCarHeightM,
				Tank::Physics::kPassengerCarLengthM) *
			XMMatrixRotationY(obstacle.yawRadians) *
			XMMatrixTranslation(
				obstacle.position.x,
				obstacle.position.y,
				obstacle.position.z),
			obstacleMaterial);
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
	m_appliedTrackedVehicleSettings = m_trackedVehicleSettings;
	m_appliedEnvironmentSettings = m_environmentSettings;
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
		{ m_trackedVehicleModel.hullUpper,
			XMMatrixScaling(0.9f * chassisWidth, 0.25f, 0.875f * chassisLength) *
				XMMatrixTranslation(0.0f, 0.125f, 0.0f) },
		{ m_trackedVehicleModel.hullLower,
			XMMatrixScaling(0.9f * chassisWidth, 0.25f, 0.875f * chassisLength) *
				XMMatrixTranslation(0.0f, -0.125f, 0.0f) },
		{ m_trackedVehicleModel.upperStructureUpper,
			XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, 0.4375f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.upperStructureLower,
			XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, 0.3125f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.lowerStructureUpper,
			XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, -0.3125f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.lowerStructureLower,
			XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
				XMMatrixTranslation(0.0f, -0.4375f, 0.075f * chassisLength) },
		{ m_trackedVehicleModel.leftTrack,
			m_showTrackProxies
				? XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, chassisLength) *
					XMMatrixTranslation(
						-0.5f * m_trackedVehicleSettings.trackSpacingM,
						0.0f,
						0.0f)
				: XMMatrixScaling(0.0f, 0.0f, 0.0f) },
		{ m_trackedVehicleModel.rightTrack,
			m_showTrackProxies
				? XMMatrixScaling(m_trackedVehicleSettings.trackWidthM, 0.5f, chassisLength) *
					XMMatrixTranslation(
						0.5f * m_trackedVehicleSettings.trackSpacingM,
						0.0f,
						0.0f)
				: XMMatrixScaling(0.0f, 0.0f, 0.0f) },
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

	const float trackRadius = (std::max)(
		m_trackedVehicleSettings.endWheelRadiusM,
		m_trackedVehicleSettings.roadWheelRadiusM);
	const float halfTrackSpacing = 0.5f * m_trackedVehicleSettings.trackSpacingM;
	const XMMATRIX inverseBodyTransform = XMMatrixInverse(nullptr, bodyTransform);
	std::array<std::vector<TrackPathPoint>, Tank::Physics::kTankTrackCount> trackPaths;
	std::array<float, Tank::Physics::kTankTrackCount> trackPerimeters = {};
	for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
	{
		trackPaths[static_cast<size_t>(track)] = BuildTrackPathFromWheels(
			state,
			m_trackedVehicleSettings,
			track,
			inverseBodyTransform);
		if (trackPaths[static_cast<size_t>(track)].size() < 2)
		{
			trackPaths[static_cast<size_t>(track)] =
				BuildTrackPath(chassisLength, trackRadius);
		}
		trackPerimeters[static_cast<size_t>(track)] =
			trackPaths[static_cast<size_t>(track)].back().distanceFromStart;
	}
	const float trackDeltaTime = std::clamp(
		state.timeSeconds - m_trackShoeLastTimeSeconds,
		0.0f,
		0.1f);
	m_trackShoeLastTimeSeconds = state.timeSeconds;

	const XMVECTOR bodyRotation = XMVectorSet(
		state.bodyRotation.x,
		state.bodyRotation.y,
		state.bodyRotation.z,
		state.bodyRotation.w);
	const XMVECTOR bodyForward =
		XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), bodyRotation);
	const XMVECTOR bodyUp =
		XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), bodyRotation);
	const XMVECTOR linearVelocity = XMVectorSet(
		state.linearVelocity.x,
		state.linearVelocity.y,
		state.linearVelocity.z,
		0.0f);
	const XMVECTOR angularVelocity = XMVectorSet(
		state.angularVelocity.x,
		state.angularVelocity.y,
		state.angularVelocity.z,
		0.0f);
	const float forwardSpeed =
		XMVectorGetX(XMVector3Dot(linearVelocity, bodyForward));
	const float yawSpeed =
		XMVectorGetX(XMVector3Dot(angularVelocity, bodyUp));
	const std::array<float, Tank::Physics::kTankTrackCount> trackSpeeds = {
		forwardSpeed + yawSpeed * halfTrackSpacing,
		forwardSpeed - yawSpeed * halfTrackSpacing };
	for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
	{
		m_trackShoeDistances[static_cast<size_t>(track)] -=
			trackSpeeds[static_cast<size_t>(track)] * trackDeltaTime;
		m_trackShoeDistances[static_cast<size_t>(track)] = std::fmod(
			m_trackShoeDistances[static_cast<size_t>(track)],
			trackPerimeters[static_cast<size_t>(track)]);
	}

	for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
	{
		const std::vector<TrackPathPoint>& trackPath =
			trackPaths[static_cast<size_t>(track)];
		const float trackPerimeter = trackPerimeters[static_cast<size_t>(track)];
		const float shoeLength =
			0.82f * trackPerimeter /
			static_cast<float>(TrackedVehicleModel::kTrackShoeCountPerTrack);
		const float trackX = track == 0 ? -halfTrackSpacing : halfTrackSpacing;
		for (int shoe = 0; shoe < TrackedVehicleModel::kTrackShoeCountPerTrack; ++shoe)
		{
			Engine::InstanceData& instance =
				scene.instances[m_trackedVehicleModel.trackShoes[static_cast<size_t>(track)]
					[static_cast<size_t>(shoe)]];
			if (!m_trackShoeDisplay)
			{
				SetInstanceWorld(instance, XMMatrixScaling(0.0f, 0.0f, 0.0f));
				continue;
			}

			const float shoeDistance =
				(static_cast<float>(shoe) + 0.5f) * trackPerimeter /
					static_cast<float>(TrackedVehicleModel::kTrackShoeCountPerTrack) +
				m_trackShoeDistances[static_cast<size_t>(track)];
			const TrackShoePose pose =
				CalculateTrackShoePose(trackPath, shoeDistance);
			const XMMATRIX shoeLocal = MakeTrackShoeLocalTransform(
				trackX,
				pose,
				m_trackedVehicleSettings.trackWidthM + 0.08f,
				0.08f,
				shoeLength);
			SetInstanceWorld(instance, shoeLocal * bodyTransform);
		}
	}

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		Engine::InstanceData& inst =
			scene.instances[m_trackedVehicleModel.wheels[static_cast<size_t>(i)]];
		inst.prevWorld = inst.world;

		if (i < state.wheelCount)
		{
			const Tank::Physics::TrackedWheelState& wheel = state.wheels[static_cast<size_t>(i)];
			SetInstanceWorld(inst, XMMatrixScaling(0.0f, 0.0f, 0.0f));

			const XMVECTOR wheelRotation = XMVectorSet(
				wheel.transform.rotation.x,
				wheel.transform.rotation.y,
				wheel.transform.rotation.z,
				wheel.transform.rotation.w);
			const int wheelsPerSurface = m_trackedVehicleSettings.roadWheelCount + 2;
			const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
			const bool endWheel =
				wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
			const float radius = endWheel
				? m_trackedVehicleSettings.endWheelRadiusM
				: m_trackedVehicleSettings.roadWheelRadiusM;
			const XMMATRIX wheelWorld =
				XMMatrixScaling(
					radius,
					m_trackedVehicleSettings.trackWidthM,
					radius) *
				XMMatrixRotationQuaternion(wheelRotation) *
				XMMatrixTranslation(
					wheel.transform.position.x,
					wheel.transform.position.y,
					wheel.transform.position.z);
			SetInstanceWorld(inst, wheelWorld);
			inst.materialId =
				m_tankVisualSettings.colorWheelsByContact && wheel.hasContact
				? m_trackedVehicleModel.contactedWheelMaterial
				: m_trackedVehicleModel.wheelMaterial;
		}
		else
		{
			SetInstanceWorld(inst, XMMatrixScaling(0.0f, 0.0f, 0.0f));
			inst.materialId = m_trackedVehicleModel.wheelMaterial;
		}
	}

	for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
	{
		const size_t wheelIndex = static_cast<size_t>(i);
		Engine::InstanceData& suspensionLine =
			scene.instances[m_trackedVehicleModel.suspensionLines[wheelIndex]];
		Engine::InstanceData& contactMarker =
			scene.instances[m_trackedVehicleModel.contactMarkers[wheelIndex]];
		Engine::InstanceData& contactNormalLine =
			scene.instances[m_trackedVehicleModel.contactNormalLines[wheelIndex]];

		if (!m_physicsDebugOverlay || i >= state.wheelCount)
		{
			const XMMATRIX hidden = XMMatrixScaling(0.0f, 0.0f, 0.0f);
			SetInstanceWorld(suspensionLine, hidden);
			SetInstanceWorld(contactMarker, hidden);
			SetInstanceWorld(contactNormalLine, hidden);
			continue;
		}

		const Tank::Physics::TrackedWheelState& wheel = state.wheels[wheelIndex];
		const int wheelsPerSurface = m_trackedVehicleSettings.roadWheelCount + 2;
		const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
		const bool endWheel =
			wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
		const float wheelRadius = endWheel
			? m_trackedVehicleSettings.endWheelRadiusM
			: m_trackedVehicleSettings.roadWheelRadiusM;
		const Tank::Physics::Vec3 suspensionEnd = {
			wheel.suspensionOrigin.x +
				wheel.suspensionDirection.x *
					(wheel.suspensionLength + wheelRadius),
			wheel.suspensionOrigin.y +
				wheel.suspensionDirection.y *
					(wheel.suspensionLength + wheelRadius),
			wheel.suspensionOrigin.z +
				wheel.suspensionDirection.z *
					(wheel.suspensionLength + wheelRadius) };
		SetInstanceWorld(
			suspensionLine,
			MakeLineTransform(wheel.suspensionOrigin, suspensionEnd, 0.06f));

		const Tank::Physics::Vec3 markerPosition = wheel.hasContact
			? Tank::Physics::Vec3 {
				wheel.contactPosition.x + wheel.contactNormal.x * 0.08f,
				wheel.contactPosition.y + wheel.contactNormal.y * 0.08f,
				wheel.contactPosition.z + wheel.contactNormal.z * 0.08f }
			: wheel.transform.position;
		SetInstanceWorld(
			contactMarker,
			XMMatrixScaling(0.22f, 0.22f, 0.22f) *
			XMMatrixTranslation(markerPosition.x, markerPosition.y, markerPosition.z));
		contactMarker.materialId = wheel.hasContact
			? m_trackedVehicleModel.debugContactMaterial
			: m_trackedVehicleModel.debugAirborneMaterial;

		if (wheel.hasContact)
		{
			const Tank::Physics::Vec3 normalStart = {
				wheel.contactPosition.x + wheel.contactNormal.x * 0.08f,
				wheel.contactPosition.y + wheel.contactNormal.y * 0.08f,
				wheel.contactPosition.z + wheel.contactNormal.z * 0.08f };
			const Tank::Physics::Vec3 normalEnd = {
				normalStart.x + wheel.contactNormal.x * 2.0f,
				normalStart.y + wheel.contactNormal.y * 2.0f,
				normalStart.z + wheel.contactNormal.z * 2.0f };
			SetInstanceWorld(
				contactNormalLine,
				MakeLineTransform(normalStart, normalEnd, 0.06f));
		}
		else
		{
			SetInstanceWorld(
				contactNormalLine,
				XMMatrixScaling(0.0f, 0.0f, 0.0f));
		}
	}

	if (!m_trackedVehicleCameraFollow)
	{
		m_debugCameraController.SetObjectViewerState(
			m_debugCameraController.ObjectViewerYaw(),
			m_debugCameraController.ObjectViewerPitch(),
			m_debugCameraController.ObjectViewerDistance(),
			{
				state.bodyPosition.x,
				state.bodyPosition.y + 0.5f,
				state.bodyPosition.z });
	}
	m_sceneRenderer.SetScene(scene);
}

void TankSandboxApp::UpdateTrackedVehicleFollowCamera(
	const Tank::Physics::TrackedVehicleTestState& state,
	float deltaTimeSeconds)
{
	if (!m_trackedVehicleCameraFollow || deltaTimeSeconds <= 0.0f)
	{
		return;
	}

	Engine::Scene& scene = m_trackedVehicleSceneBuilder.GetScene();
	Engine::CameraState& camera = scene.camera;
	const XMVECTOR bodyRotation = XMQuaternionNormalize(XMVectorSet(
		state.bodyRotation.x,
		state.bodyRotation.y,
		state.bodyRotation.z,
		state.bodyRotation.w));
	const XMVECTOR forward = XMVector3Normalize(
		XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), bodyRotation));
	const XMVECTOR pivot = XMVectorSet(
		state.bodyPosition.x,
		state.bodyPosition.y + 0.5f,
		state.bodyPosition.z,
		1.0f);
	XMVECTOR position = XMLoadFloat3(&camera.pos);
	XMVECTOR velocity = XMLoadFloat3(&m_trackedVehicleCameraVelocity);
	const float positionSpeed =
		std::clamp(m_trackedVehicleCameraPositionSpeed, 0.5f, 20.0f);
	const float damping = std::clamp(m_trackedVehicleCameraDamping, 0.1f, 2.0f);
	const float dt = std::min(deltaTimeSeconds, 1.0f / 30.0f);
	const float rotationAlpha =
		1.0f -
		std::exp(
			-std::clamp(m_trackedVehicleCameraRotationSpeed, 0.5f, 20.0f) *
			dt / damping);
	XMFLOAT3 forwardVector = {};
	XMStoreFloat3(&forwardVector, forward);
	const float desiredRearYaw =
		std::atan2(-forwardVector.x, -forwardVector.z);
	if (!m_trackedVehicleCameraOrbitInitialized)
	{
		XMFLOAT3 pivotPosition = {};
		XMStoreFloat3(&pivotPosition, pivot);
		m_trackedVehicleCameraOrbitYaw = std::atan2(
			camera.pos.x - pivotPosition.x,
			camera.pos.z - pivotPosition.z);
		m_trackedVehicleCameraYawVelocity = 0.0f;
		m_trackedVehicleCameraOrbitInitialized = true;
	}
	const float yawDelta = std::remainder(
		desiredRearYaw - m_trackedVehicleCameraOrbitYaw,
		XM_2PI);
	const float yawSpeedLimit =
		XMConvertToRadians(std::clamp(
			m_trackedVehicleCameraYawSpeedLimitDegrees,
			15.0f,
			720.0f));
	const float desiredYawVelocity = std::clamp(
		yawDelta *
			std::clamp(m_trackedVehicleCameraRotationSpeed, 0.5f, 20.0f),
		-yawSpeedLimit,
		yawSpeedLimit);
	const float yawVelocityAlpha =
		1.0f -
		std::exp(
			-std::clamp(m_trackedVehicleCameraYawDamping, 0.5f, 30.0f) * dt);
	m_trackedVehicleCameraYawVelocity +=
		(desiredYawVelocity - m_trackedVehicleCameraYawVelocity) *
		yawVelocityAlpha;
	const float yawStep = m_trackedVehicleCameraYawVelocity * dt;
	if (std::abs(yawStep) >= std::abs(yawDelta))
	{
		m_trackedVehicleCameraOrbitYaw = desiredRearYaw;
		m_trackedVehicleCameraYawVelocity = 0.0f;
	}
	else
	{
		m_trackedVehicleCameraOrbitYaw += yawStep;
	}
	const float lookDownRadians =
		XMConvertToRadians(
			std::clamp(m_trackedVehicleCameraLookDownDegrees, 0.0f, 89.0f));
	const float horizontalDistance =
		std::cos(lookDownRadians) * m_trackedVehicleCameraFollowDistance;
	const float verticalDistance =
		std::sin(lookDownRadians) * m_trackedVehicleCameraFollowDistance;
	const XMVECTOR desiredPosition =
		pivot +
		XMVectorSet(
			std::sin(m_trackedVehicleCameraOrbitYaw) *
				horizontalDistance,
			verticalDistance,
			std::cos(m_trackedVehicleCameraOrbitYaw) *
				horizontalDistance,
			0.0f);
	const XMVECTOR acceleration =
		(desiredPosition - position) * (positionSpeed * positionSpeed) -
		velocity * (2.0f * damping * positionSpeed);
	velocity += acceleration * dt;
	position += velocity * dt;
	XMStoreFloat3(&camera.pos, position);
	XMStoreFloat3(&m_trackedVehicleCameraVelocity, velocity);
	if (m_trackedVehicleCameraFovSpringActive)
	{
		const float fovAcceleration =
			(m_trackedVehicleCameraFovTarget - camera.fov) *
				(positionSpeed * positionSpeed) -
			m_trackedVehicleCameraFovVelocity *
				(2.0f * damping * positionSpeed);
		m_trackedVehicleCameraFovVelocity += fovAcceleration * dt;
		camera.fov += m_trackedVehicleCameraFovVelocity * dt;
		if (std::abs(m_trackedVehicleCameraFovTarget - camera.fov) < 0.001f &&
			std::abs(m_trackedVehicleCameraFovVelocity) < 0.001f)
		{
			camera.fov = m_trackedVehicleCameraFovTarget;
			m_trackedVehicleCameraFovVelocity = 0.0f;
			m_trackedVehicleCameraFovSpringActive = false;
		}
	}

	const XMVECTOR desiredGaze = pivot;
	const XMVECTOR currentGaze = XMLoadFloat3(&camera.gazePoint);
	XMStoreFloat3(
		&camera.gazePoint,
		XMVectorLerp(currentGaze, desiredGaze, rotationAlpha));
	m_sceneRenderer.SetCamera(camera);
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
