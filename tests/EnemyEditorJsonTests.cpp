#include "Physics/EnemyEditorJson.h"
#include <iostream>

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    auto check = [&](bool value, const char* message) { if (!value) { passed = false; std::cerr << message << '\n'; } };
    EnemyEditorSettings original;
    original.attackTypes[0].name = "A";
    original.attackTypes.push_back({});
    original.attackTypes[1].name = "B";
    original.attackTypes[1].projectileSpeedMetersPerSecond = 7;
    original.unitTypes[0].attackMounts.push_back({{2, 1, -3}, 1});
    std::string text, error;
    check(SerializeEnemyEditor(original, text, error), "serialize");
    EnemyEditorSettings loaded;
    check(DeserializeEnemyEditor(text, loaded, error) && loaded.attackTypes.size() == 2 &&
        loaded.attackTypes[1].projectileSpeedMetersPerSecond == 7 && loaded.unitTypes[0].attackMounts.size() == 2 &&
        loaded.unitTypes[0].attackMounts[1].attackTypeIndex == 1 && loaded.unitTypes[0].attackMounts[1].localPosition.z == -3,
        "multiple templates and mounts survive round trip");
    const std::string reordered = R"({"version":1,"attackTypes":[{"name":"B"},{"name":"A"}],"unitTypes":[{"name":"Tank","attackMounts":[{"attackType":"A","localPosition":[0,0,0]}]}]})";
    check(DeserializeEnemyEditor(reordered, loaded, error) && loaded.unitTypes[0].attackMounts[0].attackTypeIndex == 1 &&
        loaded.attackTypes[0].detectionRangeMeters == 50, "named references survive reordering; missing parameters use defaults");
    for (const auto invalid : {"{}", "[]", "{", R"({"version":2,"attackTypes":[],"unitTypes":[]})",
        R"({"version":1,"attackTypes":[],"unitTypes":[]})",
        R"({"version":1,"attackTypes":[{"name":"A"},{"name":"A"}],"unitTypes":[]})",
        R"({"version":1,"attackTypes":[{"name":"A","reachMeters":99}],"unitTypes":[]})",
        R"({"version":1,"attackTypes":[{"name":"A","firingIntervalSeconds":"3"}],"unitTypes":[]})",
        R"({"version":1,"attackTypes":[{"name":"A"}],"unitTypes":[{"name":"Tank","attackMounts":[{"attackType":"Missing","localPosition":[0,0,0]}]}]})"})
    {
        loaded = original;
        check(!DeserializeEnemyEditor(invalid, loaded, error) && !error.empty() && loaded.attackTypes.size() == 2 &&
            loaded.attackTypes[1].projectileSpeedMetersPerSecond == 7, "failed load preserves catalog");
    }
    original.unitTypes[0].attackMounts[0].attackTypeIndex = -1;
    text = "unchanged";
    check(!SerializeEnemyEditor(original, text, error) && text == "unchanged", "bad reference rejects save");
    if (!passed) return 1;
    std::cout << "PASS EnemyEditorJson\n";
}
