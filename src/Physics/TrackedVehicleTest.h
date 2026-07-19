#pragma once

#include "PhysicsTypes.h"
#include "TankTypes.h"

#include <memory>

namespace Tank::Physics
{
    struct TrackedVehicleTestState
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        Vec3 bodyPosition = {};
        Quat bodyRotation = {};
        Vec3 linearVelocity = {};
        Vec3 angularVelocity = {};
        bool sleeping = false;
    };

    class TrackedVehicleTest
    {
    public:
        TrackedVehicleTest();
        ~TrackedVehicleTest();

        TrackedVehicleTest(const TrackedVehicleTest&) = delete;
        TrackedVehicleTest& operator=(const TrackedVehicleTest&) = delete;

        void Initialize();
        void SetInput(const TankInput& input);
        TrackedVehicleTestState Step(float deltaTimeSeconds);
        const TrackedVehicleTestState& State() const { return m_state; }

    private:
        struct Impl;

        TrackedVehicleTestState m_state = {};
        std::unique_ptr<Impl> m_impl;
    };
}
