#include "Physics/PlayerCombatState.h"

#include <iostream>
#include <limits>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL PlayerCombatState: " << message << '\n';
        return condition;
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    std::string error;
    PlayerCombatState player;
    passed &= Check(player.Snapshot().hitPoints == 100 && player.Snapshot().lives == 3, "default state");
    passed &= Check(!player.Continue() && !player.Respawn(), "alive rejects recovery commands");
    passed &= Check(!player.ApplyDamage(-1, {}) && !player.ApplyDamage(0, {}) &&
        !player.ApplyDamage(std::numeric_limits<float>::infinity(), {}) &&
        !player.ApplyDamage(100, {std::numeric_limits<float>::quiet_NaN(), 0, 0}), "invalid hits rejected");
    passed &= Check(player.ApplyDamage(25, {}) && player.Snapshot().hitPoints == 75 && player.Snapshot().lives == 3,
        "nonlethal damage retains life");
    for (int loss = 1; loss <= 3; ++loss)
    {
        const Vec3 position {static_cast<float>(loss), 2, 7};
        passed &= Check(player.ApplyEnemyContact(position), "enemy contact accepted");
        passed &= Check(player.Snapshot().hitPoints == 0 && player.Snapshot().lives == 3 - loss &&
            player.Snapshot().lostPosition.x == position.x && player.Snapshot().lostPosition.y == 2 &&
            player.Snapshot().lostPosition.z == 7, "loss records position and spends exactly one life");
        passed &= Check(!player.ApplyEnemyContact({}) && player.Snapshot().lives == 3 - loss,
            "duplicate hits cannot consume another life before recovery");
        if (loss < 3)
        {
            passed &= Check(player.Snapshot().phase == PlayerCombatPhase::Lost && !player.Continue(), "Lost phase");
            passed &= Check(player.Respawn() && player.Snapshot().hitPoints == 100 &&
                player.Snapshot().lostPosition.x == position.x && !player.Respawn(), "explicit respawn retains location");
        }
    }
    passed &= Check(player.Snapshot().phase == PlayerCombatPhase::GameOver && !player.Respawn(), "third loss is GameOver");
    passed &= Check(player.Continue() && player.Snapshot().lives == 3 && player.Snapshot().hitPoints == 100 &&
        player.Snapshot().phase == PlayerCombatPhase::Alive && !player.Continue(), "Continue resets session");
    passed &= Check(!player.Initialize({0, 3, 100}, error) && player.Snapshot().lives == 3, "invalid initialize is atomic");
    passed &= Check(player.Initialize({200, 2, 50}, error) && player.ApplyEnemyContact({}) &&
        player.Snapshot().hitPoints == 150 && player.Snapshot().lives == 2, "custom tuning used");
    passed &= Check(player.ApplyDamage(1000, {}) && player.Snapshot().hitPoints == 0, "overkill clamps HP");
    if (!passed) return 1;
    std::cout << "PASS PlayerCombatState\n";
}
