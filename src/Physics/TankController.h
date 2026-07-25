#pragma once

#include "TankTypes.h"

#include <memory>

namespace Tank::Physics
{
    class PhysicsWorld;

    class TankController
    {
    public:
        TankController();
        ~TankController();

        TankController(const TankController&) = delete;
        TankController& operator=(const TankController&) = delete;

        void Initialize();
        void Initialize(PhysicsWorld& world);
        void Initialize(PhysicsWorld& world, const TankSettings& settings);
        void SetInput(const TankInput& input);
        void PreStep();
        void PostStep(float deltaTimeSeconds);

        const TankInput& Input() const { return m_input; }
        const TankState& State() const { return m_state; }
        const TankSettings& Settings() const { return m_settings; }

    private:
        struct Impl;

        TankInput m_input = {};
        TankState m_state = {};
        TankSettings m_settings = {};
        std::unique_ptr<Impl> m_impl;
    };
}
