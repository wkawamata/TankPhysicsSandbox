#include "Physics/CombatSettings.h"

#include <iostream>
#include <limits>

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    auto check = [&](bool condition, const char* message)
    {
        if (!condition) { std::cerr << "FAIL CombatSettings: " << message << '\n'; passed = false; }
    };
    CombatSettings settings {250, 5, 25};
    std::string text, error;
    check(SerializeCombatSettings(settings, text, error), "serialize custom tuning");
    CombatSettings restored;
    check(DeserializeCombatSettings(text, restored, error) && restored.maximumHitPoints == 250 &&
        restored.startingLives == 5 && restored.enemyContactDamage == 25, "round trip");
    check(DeserializeCombatSettings("{}", restored, error) && restored.maximumHitPoints == 100 &&
        restored.startingLives == 3 && restored.enemyContactDamage == 100, "missing fields use defaults");
    for (const auto invalid : {"[]", "{", "{\"version\":2}", "{\"maximumHitPoints\":0}",
        "{\"maximumHitPoints\":\"100\"}", "{\"maximumHitPoints\":1e100}", "{\"startingLives\":0}",
        "{\"startingLives\":3.5}", "{\"startingLives\":18446744073709551615}",
        "{\"enemyContactDamage\":-1}", "{\"enemyContactDamage\":null}"})
    {
        restored = settings;
        check(!DeserializeCombatSettings(invalid, restored, error) && !error.empty() &&
            restored.maximumHitPoints == 250 && restored.startingLives == 5 && restored.enemyContactDamage == 25,
            "invalid JSON rejected without partial update");
    }
    text = "unchanged";
    check(!SerializeCombatSettings({std::numeric_limits<float>::infinity(), 3, 100}, text, error) &&
        text == "unchanged", "nonfinite settings rejected");
    check(DeserializeCombatSettings("{\"enemyContactDamage\":0}", restored, error) &&
        restored.enemyContactDamage == 0, "zero contact damage permitted");
    if (!passed) return 1;
    std::cout << "PASS CombatSettings\n";
}
