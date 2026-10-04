#pragma once
#include "EnemyAttackType.h"

namespace Tank::Physics
{
    bool SerializeEnemyEditor(const EnemyEditorSettings& settings, std::string& text, std::string& error);
    bool DeserializeEnemyEditor(const std::string& text, EnemyEditorSettings& settings, std::string& error);
}
