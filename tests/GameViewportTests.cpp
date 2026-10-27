#include "App/GameViewportJson.h"
#include "Engine/SceneViewport.h"
#include "App/SnapshotUndoEdit.h"
#include <iostream>
#include <stdexcept>
#include <limits>

int main()
{
    using namespace Tank::App;
    try
    {
        const auto check = [](bool v, const char* message) {if (!v) throw std::runtime_error(message);};
        GameViewportSettings s;
        auto r = ResolveGameViewport(s, 1920, 1080);
        check(r.x == 0 && r.width == 1920 && r.height == 1080, "Default uses entire window");
        s.enabled = true; s.x = .1f; s.y = .2f; s.width = .8f; s.height = .6f;
        r = ResolveGameViewport(s, 1000, 1000);
        check(r.x == 100 && r.y == 200 && r.width == 800 && r.height == 600, "Normalized window coordinates and size");
        check(r.Contains(100,200) && !r.Contains(900,200) && !r.Contains(99,200), "Black bars reject mouse gestures");
        r = ResolveGameViewport(s, 2000, 1000);
        check(r.x == 200 && r.y == 200 && r.width == 1600, "Resize recalculates normalized rectangle");
        s.x = .9f; s.y = .9f;
        r = ResolveGameViewport(s, 1000, 1000);
        check(r.width == 100 && r.height == 100, "Overflow is clipped to window");
        s.x = 1; s.y = 1;
        r = ResolveGameViewport(s, 1000, 1000);
        check(r.x == 999 && r.y == 999 && r.width == 1 && r.height == 1, "Edge endpoints remain valid");
        s = {}; s.enabled = true; s.aspect = 2;
        r = ResolveGameViewport(s, 1920, 1080);
        check(r.width == 1440 && r.height == 1080, "4:3 rectangle inside 16:9");
        auto output = Engine::ResolveSceneViewport({240, 0, 1440, 1080}, 1920, 1080);
        check(output.x == 240 && output.width == 1440 && output.height == 1080, "Renderer uses viewport-sized output buffers");
        auto moved = Engine::ResolveSceneViewport({400, 0, 1440, 1080}, 1920, 1080);
        check(moved.width == output.width && moved.height == output.height, "Position alone preserves buffer dimensions");
        output = Engine::ResolveSceneViewport({}, 1920, 1080);
        check(output.width == 1920 && output.height == 1080 && output.x == 0, "Renderer reset restores full-window buffers");
        output = Engine::ResolveSceneViewport({9999, 9999, 1920, 1080}, 800, 600);
        check(output.x == 799 && output.y == 599 && output.width == 1 && output.height == 1, "Renderer clamps resized viewport to window");
        output = Engine::ResolveSceneViewport({20, 30, 960, 540}, 0, 0);
        check(output.width == 0 && output.height == 0, "Minimized renderer stays zero-sized");
        s.aspect = 3;
        r = ResolveGameViewport(s, 0, 0); check(r.width == 1 && r.height == 1, "Minimized window is safe");
        s.x = std::numeric_limits<float>::quiet_NaN();
        r = ResolveGameViewport(s, 1000, 1000); check(r.x == 0, "Nonfinite coordinate uses safe fallback");
        s = {}; s.enabled = true; s.x = .1f; s.width = .8f; s.aspect = 1;
        std::string error;
        GameViewportSettings loaded;
        check(DeserializeGameViewport(SerializeGameViewport(s), loaded, error) && loaded == s, "Round-trip global viewport settings");
        const auto valid = loaded;
        check(!DeserializeGameViewport("{}", loaded, error) && loaded == valid, "Invalid JSON preserves current settings");
        auto bad = nlohmann::json::parse(SerializeGameViewport(s)); bad["x"] = 2;
        check(!DeserializeGameViewport(bad.dump(), loaded, error), "Reject coordinates above one");
        bad["x"] = 0; bad["width"] = 0;
        check(!DeserializeGameViewport(bad.dump(), loaded, error), "Reject zero-sized saved viewport");
        UndoHistory history; SnapshotUndoEdit<GameViewportSettings> edit;
        auto before = s; s.x = .2f;
        const auto apply = [&](const auto& value) {s = value;};
        edit.Update(before, s, 42, history, apply, "Viewport"); before = s; s.x = .3f;
        edit.Update(before, s, 42, history, apply, "Viewport"); edit.Flush(history, apply, "Viewport");
        check(history.UndoCount() == 1, "Continuous viewport drag forms one command");
        history.Undo(); check(s.x == .1f, "Undo viewport edit");
        history.Redo(); check(s.x == .3f, "Redo viewport edit");
        std::cout << "PASS GameViewport\n"; return 0;
    }
    catch (const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
}
