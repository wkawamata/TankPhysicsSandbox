#include "Map/GatePrefab.h"
#include "Physics/TrackedVehicleTest.h"
#include "App/ManifestUndoEdit.h"
#include <iostream>
#include <stdexcept>
#include <limits>

namespace
{
    void Check(bool valid, const char* message)
    { if (!valid) throw std::runtime_error(message); }
}

int main()
{
    using namespace Tank::Map;
    using namespace Tank::Physics;
    try
    {
        std::string error, json;
        Manifest document;
        for (int variant = 0; variant < 3; ++variant)
        {
            const auto group = AddGatePrefab(document, variant);
            Check(!group.empty(), "Create each gate size");
            Check(document.instances.back().gate->travelMeters == GateHeights[variant] + 0.2f, "Travel clears opening");
            HitTriangleMesh frame, panel;
            const auto root = std::filesystem::path(TANK_TEST_GATE_ASSETS_DIR);
            const auto& f = document.instances[document.instances.size()-2];
            Check(LoadGltfHitMesh(root / std::filesystem::path(f.asset).filename(), frame, error), "Load dummy frame collision");
            Check(LoadGltfHitMesh(root / std::filesystem::path(document.instances.back().asset).filename(), panel, error), "Load dummy shutter collision");
            float minX = 0, maxX = 0, maxY = 0;
            for (const auto& v : panel.vertices)
            {minX = std::min(minX, v[0]); maxX = std::max(maxX, v[0]); maxY = std::max(maxY, v[1]);}
            Check(maxX-minX == GateWidths[variant] && maxY == GateHeights[variant], "Dummy sizes use meters");
        }
        Check(SerializeManifest(document, json, error), "Serialize gates");
        Manifest loaded;
        Check(DeserializeManifest(json, loaded, error) && loaded.instances.size() == 6 &&
            loaded.instances[1].gate->triggerRadiusMeters == 12 && !loaded.instances[0].gate, "Round-trip gate settings and frame");
        Check(DuplicateNode(loaded, "gate-1", error).has_value() && loaded.instances.size() == 8 &&
            loaded.instances.back().gate.has_value(), "Duplicate complete gate group");
        loaded.instances.back().gate->triggerRadiusMeters = -1;
        Check(!SerializeManifest(loaded, json, error), "Reject negative gate radius");
        loaded.instances.back().gate->triggerRadiusMeters = std::numeric_limits<float>::infinity();
        Check(!SerializeManifest(loaded, json, error), "Reject nonfinite radius");
        loaded = document;
        loaded.instances[1].breakable = true;
        Check(!SerializeManifest(loaded, json, error), "Gate cannot be destructible");

        Tank::App::UndoHistory history;
        Tank::App::ManifestUndoEdit edit;
        auto authored = document;
        const auto apply = [&](const Manifest& state) {authored = state;};
        auto after = authored;
        after.instances[1].gate->triggerRadiusMeters = 8;
        edit.Update(authored, after, 123, history, apply, "Gate radius"); authored = after;
        after.instances[1].gate->triggerRadiusMeters = 15;
        edit.Update(authored, after, 123, history, apply, "Gate radius"); authored = after;
        edit.Flush(history, apply, "Gate radius");
        Check(history.UndoCount() == 1, "Radius slider is one undo command");
        history.Undo(); Check(authored.instances[1].gate->triggerRadiusMeters == 12, "Undo radius");
        history.Redo(); Check(authored.instances[1].gate->triggerRadiusMeters == 15, "Redo radius");

        GateState motion{"test", {}, {0, 3.2f, 0}, 5, 2};
        motion.Advance({0,0,6}, 1); Check(!motion.triggered && motion.fraction == 0, "Outside radius stays closed");
        motion.Advance({0,0,5}, 0.25f); Check(motion.triggered && std::abs(motion.Offset()[1]-.5f)<.001f, "Boundary triggers smooth slide");
        motion.Advance({0,0,100}, 10); Check(motion.fraction == 1, "Opens once and remains open outside radius");
        motion.Advance({}, -1); Check(motion.fraction == 1, "Invalid time cannot rewind");
        GateState vertical{"vertical", {}, {0,3,0}, 5, 2};
        vertical.Advance({0,6,0}, 1); Check(!vertical.triggered, "Radius is a 3D sphere");

        const auto root = std::filesystem::path(TANK_TEST_GATE_ASSETS_DIR);
        HitTriangleMesh panel;
        Check(LoadGltfHitMesh(root / "Gate10Shutter.gltf", panel, error), "Load shutter");
        panel.placementId = "shutter"; panel.gate = GateSettings{.5f,3.2f,2}; panel.gateTravel = {0,3.2f,0};
        HitTriangleMesh floor;
        floor.vertices = {{-100,0,-100},{100,0,-100},{100,0,100},{-100,0,100}};
        floor.triangles = {{0,2,1},{0,3,2}};
        TrackedVehicleTest world;
        const auto drive = [&]()
        {
            TankInput input; input.throttle = 1; world.SetInput(input);
            for (int i=0; i<480; ++i) world.Step(1.0f/60);
        };
        Check(world.InitializeWithStaticMeshes({}, {}, {floor,panel}, {{0,1,-8},0}, error), "Initialize closed gate physics");
        Check(world.State().gates.size() == 1 && world.State().gates[0].fraction == 0, "Game starts closed");
        drive();
        Check(!world.State().gates[0].triggered && world.State().bodyPosition.z < 0, "Closed shutter blocks driving tank");
        panel.gate->triggerRadiusMeters = 20;
        Check(world.InitializeWithStaticMeshes({}, {}, {floor,panel}, {{0,1,-8},0}, error), "Initialize opening gate physics");
        drive();
        Check(world.State().gates[0].fraction == 1 && world.State().bodyPosition.z > 5, "Tank drives through raised collision");
        Check(world.InitializeWithStaticMeshes({}, {}, {floor,panel}, {{0,1,-8},0}, error) &&
            world.State().gates[0].fraction == 0 && !world.State().gates[0].triggered, "Replay resets gate");
        std::cout << "PASS Gates\n";
        return 0;
    }
    catch (const std::exception& e)
    { std::cerr << "FAIL Gates: " << e.what() << '\n'; return 1; }
}
