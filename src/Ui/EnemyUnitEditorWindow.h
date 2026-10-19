#pragma once

#include <functional>
#include <string>

namespace Tank::Physics
{
    struct EnemyEditorSettings;
    struct TrackedVehicleTestState;
}

namespace Ui
{
    class EnemyUnitEditorWindow
    {
    public:
        bool open = false;
        void Draw(Tank::Physics::EnemyEditorSettings& editor, std::string& jsonPath,
            const Tank::Physics::TrackedVehicleTestState* state = nullptr,
            const std::function<void()>& applyAndReset = {});

    private:
        std::string m_status;
    };
}
