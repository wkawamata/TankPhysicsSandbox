#pragma once

#include "PhysicsTypes.h"
#include "PhysicsEnvironmentSettings.h"
#include "MapDefinition.h"
#include "TankTypes.h"
#include "MortarAimController.h"

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
        float speedMetersPerSecond = 0.0f;
        float maximumSpeedMetersPerSecond = 0.0f;
        float zeroToTenTimeSeconds = -1.0f;
        float engineRpm = 0.0f;
        int transmissionGear = 0;
        float clutchFriction = 0.0f;
        float yawSpeedDegrees = 0.0f;
        bool yawSpeedLimited = false;
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
        bool sleeping = false;
        TankMotionObservation motionObservation = {};
        MobilityStateSnapshot mobility = {};
        RollingPhase rollingPhase = RollingPhase::None;
        MortarAimSnapshot mortarAim = {};
        SpecialMoveStateSnapshot specialMove = {};
        bool rollingObstructionSuspected = false;
        bool rollingRecoveryActive = false;
        bool trackInputSwapped = false;
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
            const std::vector<MapPrimitive>& mapPrimitives = {},
            const MapSpawn& spawn = {});
        void SetInput(const TankInput& input);
        bool ApplyConfiguredRecoil();
        bool ApplyRecoilImpulse(float impulseNewtonSeconds);
        TrackedVehicleTestState Step(float deltaTimeSeconds);
        const TrackedVehicleTestState& State() const { return m_state; }
        const TankInput& Input() const;
        const TrackedDriverInput& DriverInput() const;
        const TankSettings& Settings() const;

    private:
        struct Impl;

        TrackedVehicleTestState m_state = {};
        std::unique_ptr<Impl> m_impl;
    };
}
