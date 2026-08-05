#pragma once

#include "PhysicsTypes.h"
#include "PhysicsEnvironmentSettings.h"
#include "MapDefinition.h"
#include "TankTypes.h"

#include <array>
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
        float yawSpeedDegrees = 0.0f;
        bool yawSpeedLimited = false;
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
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
        void Initialize(const TankSettings& settings);
        void Initialize(
            const TankSettings& settings,
            const PhysicsEnvironmentSettings& environmentSettings,
            const std::vector<MapPrimitive>& mapPrimitives = {});
        void SetInput(const TankInput& input);
        TrackedVehicleTestState Step(float deltaTimeSeconds);
        const TrackedVehicleTestState& State() const { return m_state; }
        const TrackedDriverInput& DriverInput() const;
        const TankSettings& Settings() const;

    private:
        struct Impl;

        TrackedVehicleTestState m_state = {};
        std::unique_ptr<Impl> m_impl;
    };
}
