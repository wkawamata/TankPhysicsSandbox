#include "stdafx.h"
#include "TankSandboxApp.h"
#include "Platform/Win32Application.h"
#include <cstdio>
#include <exception>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    try
    {
        TankSandboxApp sample(1920, 1080, L"Tank Physics Sandbox");
        return Win32Application::Run(&sample, hInstance, nCmdShow);
    }
    catch (const std::exception& error)
    {
        // Automation must fail with a nonzero exit code instead of opening a
        // blocking error dialog for invalid capture settings.
        std::fprintf(stderr, "TankSandbox: %s\n", error.what());
        OutputDebugStringA(error.what());
        return 1;
    }
}
