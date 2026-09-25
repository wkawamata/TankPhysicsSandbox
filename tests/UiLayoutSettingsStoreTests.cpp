#include "App/UiLayoutSettingsStore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

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
    source.telemetryWindowVisible = false;
    source.rollingParametersWindowVisible = false;
    source.gamepadInputWindowVisible = false;
    source.renderSettingsWindowVisible = false;
    source.outputWindowVisible = false;
    source.rollingCheatWindowVisible = true;
    source.rollingCheatFontScale = 1.5f;

    std::string status;
    passed &= Check(store.Write(source, status), "write must succeed");
    std::ifstream savedFile(store.Path(), std::ios::binary);
    const std::string savedText(
        (std::istreambuf_iterator<char>(savedFile)),
        std::istreambuf_iterator<char>());
    savedFile.close();
    bool usesOnlyCrlfLineEndings = true;
    for (std::size_t index = 0; index < savedText.size(); ++index)
    {
        if (savedText[index] == '\n' &&
            (index == 0 || savedText[index - 1] != '\r'))
        {
            usesOnlyCrlfLineEndings = false;
            break;
        }
    }
    passed &= Check(usesOnlyCrlfLineEndings, "saved JSON must use CRLF line endings");
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
