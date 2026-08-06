#pragma once

#include "TankTypes.h"
#include "MapDefinition.h"

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
        void Initialize(
            PhysicsWorld& world,
            const TankSettings& settings,
            const MapSpawn& spawn);
        void SetInput(const TankInput& input);
        bool ApplyConfiguredRecoil();
        bool ApplyRecoilImpulse(float impulseNewtonSeconds);
        void PreStep();
        void PostStep(float deltaTimeSeconds);

        const TankInput& Input() const { return m_input; }
        const TrackedDriverInput& DriverInput() const { return m_driverInput; }
        const TankState& State() const { return m_state; }
        const TankSettings& Settings() const { return m_settings; }

    private:
        struct Impl;

        TankInput m_input = {};
        TrackedDriverInput m_driverInput = {};
        TankState m_state = {};
        TankSettings m_settings = {};
        std::unique_ptr<Impl> m_impl;
    };
}
