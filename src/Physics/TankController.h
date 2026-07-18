#pragma once

#include "TankTypes.h"

namespace Tank::Physics
{
    class TankController
    {
    public:
        void Initialize();
        void SetInput(const TankInput& input);
        void Step(float deltaTimeSeconds);

        const TankInput& Input() const { return m_input; }
        const TankState& State() const { return m_state; }

    private:
        TankInput m_input = {};
        TankState m_state = {};
    };
}
