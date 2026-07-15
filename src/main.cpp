#include "stdafx.h"
#include "TankSandboxApp.h"
#include "Platform/Win32Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    TankSandboxApp sample(1920, 1080, L"Tank Physics Sandbox");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
