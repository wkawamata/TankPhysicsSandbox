#include "App/UiLayoutSettingsStore.h"
#include "App/UiVisibilityUndo.h"

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
    source.performanceMeterVisible = false;
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

    // Existing settings files have no Performance Meter field; preserve the default ON.
    std::ofstream legacy(store.Path(), std::ios::binary | std::ios::trunc);
    legacy << R"({"version":1,"cameraWindowVisible":false})";
    legacy.close();
    Tank::App::UiLayoutSettings legacyLoaded;
    passed &= Check(store.Read(legacyLoaded, status), "legacy JSON must load");
    passed &= Check(legacyLoaded.performanceMeterVisible && !legacyLoaded.cameraWindowVisible,
        "missing performance flag defaults to visible without changing other flags");

    passed &= Check(Tank::App::ShouldDrawToolWindow(true, true), "enabled camera is visible while tool UI is shown");
    passed &= Check(!Tank::App::ShouldDrawToolWindow(false, true), "tool UI toggle hides an enabled camera");
    passed &= Check(!Tank::App::ShouldDrawToolWindow(true, false), "tool UI toggle does not reopen a disabled camera");

    Tank::App::UndoHistory history;
    bool performanceVisible = true, cameraVisible = true;
    performanceVisible = false;
    Tank::App::RecordUiVisibilityChange(history, performanceVisible, true);
    cameraVisible = false; // An independent change must not be reverted with the meter.
    history.Undo();
    passed &= Check(performanceVisible && !cameraVisible, "undo affects only the selected window");
    history.Redo();
    passed &= Check(!performanceVisible && !cameraVisible, "redo restores the selected window");
    history.Undo();
    cameraVisible = true;
    Tank::App::RecordUiVisibilityChange(history, cameraVisible, false);
    passed &= Check(!history.CanRedo(), "a new visibility change clears the redo branch");
    history.Undo();
    passed &= Check(!cameraVisible && performanceVisible, "camera and meter changes undo independently");
    const auto count = history.UndoCount();
    Tank::App::RecordUiVisibilityChange(history, performanceVisible, performanceVisible);
    passed &= Check(history.UndoCount() == count, "unchanged checkbox creates no history item");

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
