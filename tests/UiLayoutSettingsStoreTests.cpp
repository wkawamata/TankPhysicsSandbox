#include "App/UiLayoutSettingsStore.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL UiLayoutSettingsStore: " << message << '\n';
        }
        return condition;
    }
}

int main()
{
    bool passed = true;
    const std::filesystem::path testRoot =
        std::filesystem::current_path() / "UiLayoutSettingsStoreTestsTemp";
    std::filesystem::remove_all(testRoot);

    Tank::App::UiLayoutSettingsStore store(testRoot);
    Tank::App::UiLayoutSettings source;
    source.cameraWindowVisible = false;
    source.gamepadInputWindowVisible = false;
    source.renderSettingsWindowVisible = false;
    source.outputWindowVisible = false;
    source.rollingCheatWindowVisible = true;

    std::string status;
    passed &= Check(store.Write(source, status), "write must succeed");
    Tank::App::UiLayoutSettings loaded;
    passed &= Check(store.Read(loaded, status), "read must succeed after write");
    passed &= Check(loaded == source, "visibility flags must round trip");

    std::ofstream invalid(store.Path(), std::ios::binary | std::ios::trunc);
    invalid << "{invalid}";
    invalid.close();
    passed &= Check(!store.Read(loaded, status), "invalid JSON must fail");
    passed &= Check(!status.empty(), "invalid JSON must report status");

    std::filesystem::remove_all(testRoot);
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS UiLayoutSettingsStore\n";
    return 0;
}
