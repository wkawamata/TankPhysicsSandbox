#include "stdafx.h"
#include "TankSandboxApp.h"
#include "Platform/Win32Application.h"
#include "Scene/SceneBuilder.h"
#include "imgui.h"

#include <DirectXMath.h>

using namespace DirectX;

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

    InitializeImGui();

    m_sceneRenderer.Initialize(GetWidth(), GetHeight());

    m_sceneRenderer.SetToolUiHandler([this]() { DrawToolUi(); });

    Engine::SceneBuilder builder;
    uint32_t matGray = builder.AddSolidColorMaterial(128, 128, 128, 255);
    uint32_t matRed = builder.AddSolidColorMaterial(200, 50, 50, 255);

    builder.AppendCube(1.0f, matGray);
    builder.AppendSphere(0.5f, 32, 32, matRed);

    XMMATRIX cubeWorld = XMMatrixTranslation(0.0f, 0.5f, 0.0f);
    XMMATRIX sphereWorld = XMMatrixTranslation(1.5f, 0.5f, 0.0f);

    builder.AddInstance(cubeWorld, matGray);
    builder.AddInstance(sphereWorld, matRed);

    Engine::CameraState camera;
    camera.pos = {0.0f, 1.0f, -4.0f};
    camera.gazePoint = {0.0f, 0.0f, 0.0f};
    camera.fov = 60.0f;
    camera.nearZ = 0.1f;
    camera.farZ = 100.0f;
    builder.SetCamera(camera);

    m_sceneRenderer.SetScene(builder.GetScene());
    m_sceneRenderer.ReloadSceneResources(builder.GetScene());
}

void TankSandboxApp::OnDestroy()
{
    m_sceneRenderer.Shutdown();
}

void TankSandboxApp::OnKeyDown(UINT8 key)
{
    if (key == VK_ESCAPE)
    {
        PostQuitMessage(0);
    }
}

void TankSandboxApp::OnKeyUp(UINT8)
{
}

void TankSandboxApp::OnWindowSizeChanged(UINT width, UINT height)
{
    m_windowInfo.width = width;
    m_windowInfo.height = height;
    m_windowInfo.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    m_sceneRenderer.RequestResize(width, height);
}

void TankSandboxApp::OnIdle()
{
    UpdateUiFrame();

    m_sceneRenderer.RunFrame(
        [this](ID3D12GraphicsCommandList* commandList)
        {
            m_imguiSystem.Render(commandList);
        });
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
    m_imguiSystem.EndFrame();
}

void TankSandboxApp::DrawToolUi()
{
    ImGui::Begin("Tank Sandbox");
    ImGui::Text("Hello from Tank Physics Sandbox");
    ImGui::Text("Frame: %.1f ms", m_sceneRenderer.CpuFrameTimeMs());
    ImGui::End();
}
