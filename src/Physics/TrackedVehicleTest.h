#pragma once

#include "PhysicsTypes.h"
#include "PhysicsEnvironmentSettings.h"
#include "MapDefinition.h"
#include "TankTypes.h"
#include "MortarAimController.h"
#include "Map/GltfHitMesh.h"
#include "ImpactMarkBuffer.h"
#include "EnemyAttackType.h"
#include "EnemyCombat.h"
#include "PlayerCombatState.h"
#include "EnemyGroupCommand.h"
#include "EnemyGroupBehavior.h"
#include "EnemyNavigationPlanner.h"
#include "EnemyNavigationSetup.h"
#include "Map/MapManifest.h"
#include "GateMotion.h"

#include <array>
#include <memory>
#include <vector>

namespace Tank::Physics
{
    struct DestructibleBoxState
    {
        CombatTarget target = {};
        Vec3 position = {};
        Vec3 size = {};
        std::string placementId;
    };

    struct MortarProjectileState
    {
        Vec3 position = {};
        Vec3 velocity = {};
        float blastRadiusMeters = 0.0f;
        float damage = 0.0f;
        float ageSeconds = 0.0f;
        float gravityScale = 1.0f; // Keeps a time-scaled launch on the same arc.
    };

    // Transient visual record of one detonation; aged by the physics step.
    struct MortarBlastState
    {
        Vec3 position = {};
        float radiusMeters = 0.0f;
        float ageSeconds = 0.0f;
        float durationSeconds = 0.0f;
    };

    struct EnemyRunningGearState
    {
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
        std::array<float, kTankTrackCount> trackAngularVelocityRadians = {};
    };
    // Box collision proxy for the first stationary enemy. HP is explicit tuning.
    struct FixedTurretState
    {
        CombatTarget target = {};
        Vec3 position = {};
        Vec3 size = {};
        Quat rotation = {};
        std::string placementId;
        EnemyUnitType unitType;
        std::vector<EnemyMountState> mounts;
        bool simpleMoving = false;
        bool physicalMoving = false;
        Vec3 navigationVelocity = {};
        float navigationYawRadiansPerSecond = 0;
        std::optional<EnemyRunningGearState> runningGear;
        std::string supportPlacementId;
        bool supportReleased = false;
    };

    struct EnemyGroupState
    {
        std::string id;
        std::vector<EnemyCommandMember> members;
        std::string leaderId;
        bool detected = false;
        bool hasLastKnownPosition = false;
        Vec3 lastKnownPosition = {};
        std::optional<NavigationObservation> lastNavigationObservation;
        // Independent of the player controller clock, which resets on respawn.
        double lastObservedSeconds = -1;
        std::optional<EnemyGroupBehaviorSnapshot> behavior;
        std::optional<EnemyNavigationPlan> navigation;
    };

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
        bool transmissionSwitchingGear = false;
        float leftTrackAngularVelocityRadians = 0.0f;
        float rightTrackAngularVelocityRadians = 0.0f;
        float leftTrackDriveTorqueNm = 0.0f;
        float rightTrackDriveTorqueNm = 0.0f;
        float yawSpeedDegrees = 0.0f;
        bool yawSpeedLimited = false;
        std::array<TrackedWheelState, kTankWheelCount> wheels = {};
        int wheelCount = 0;
        bool sleeping = false;
        TankMotionObservation motionObservation = {};
        MobilityStateSnapshot mobility = {};
        RollingPhase rollingPhase = RollingPhase::None;
        RollingTelemetry rollingTelemetry = {};
        RollingDecision lastRollingDecision = RollingDecision::None;
        std::uint64_t rollingDecisionCount = 0;
        float rollingDecisionCommandSign = 0.0f;
        float rollingDecisionInputSign = 0.0f;
        RollingTraceEvent lastRollingTraceEvent = RollingTraceEvent::None;
        std::uint64_t rollingTraceSequence = 0;
        float rollingTraceRequestSign = 0.0f;
        float rollingTraceCommandSign = 0.0f;
        float rollingTraceInputSign = 0.0f;
        MortarAimSnapshot mortarAim = {};
        AssaultWeaponSnapshot assaultWeapon = {};
        std::vector<AssaultProjectileState> assaultProjectiles;
        ImpactMarkBuffer assaultImpactMarks;
        std::uint64_t assaultHitTargetId = 0;
        std::vector<MortarProjectileState> mortarProjectiles;
        std::vector<MortarBlastState> mortarBlasts;
        std::uint64_t mortarShotsFired = 0;
        std::uint64_t mortarHitTargetId = 0;
        std::vector<GateState> gates;
        std::vector<DestructibleBoxState> destructibleBoxes;
        std::vector<FixedTurretState> fixedTurrets;
        std::vector<EnemyGroupState> enemyGroups;
        std::vector<EnemyProjectileState> enemyProjectiles;
        PlayerCombatSnapshot playerCombat;
        float respawnSecondsRemaining = 0;
        float invulnerabilitySecondsRemaining = 0;
        std::uint64_t respawnCount = 0;
        SpecialMoveStateSnapshot specialMove = {};
        bool rollingObstructionSuspected = false;
        bool rollingRecoveryActive = false;
        bool trackInputSwapped = false;
        bool rollChainAvailable = false;
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
        bool InitializeWithStaticMeshes(
            const TankSettings& settings,
            const PhysicsEnvironmentSettings& environmentSettings,
            const std::vector<Map::HitTriangleMesh>& staticMeshes,
            const MapSpawn& spawn,
            std::string& error);
        void SetInput(const TankInput& input);
        bool FireAssault();
        void SetAssaultProjectileSettings(const AssaultProjectileSettings& settings);
        const AssaultProjectileSettings& ProjectileSettings() const;
        bool SetEnemySupport(const std::string& enemyPlacementId, const std::string& supportPlacementId);
        bool AddDestructibleBox(const Vec3& position, const Vec3& size, float hitPoints = 60.0f);
        bool AddFixedTurret(const Vec3& position, const Vec3& size, float hitPoints, const Quat& rotation = {},
            const std::string& placementId = {}, const EnemyUnitType& unitType = {});
        void RequireNeutralInput();
        bool SetEnemySimulationSettings(const EnemySimulationSettings& settings);
        bool ConfigureEnemyMovement(const EnemyNavigationSetup& setup, std::string& error);
        bool ConfigureGroupNavigation(const std::string& groupId, const NavigationGrid& grid,
            std::span<const NavigationMobility> movingMembers, double snapDistance, const std::string& anchorSurface);
        bool ConfigureEnemyGroups(const std::vector<Map::EnemyGroup>& groups);
        bool ConfigureEnemyAttacks(const std::vector<EnemyAttackType>& types);
        bool ApplyConfiguredRecoil();
        bool ApplyRecoilImpulse(float impulseNewtonSeconds);
        TrackedVehicleTestState Step(float deltaTimeSeconds);
        const TrackedVehicleTestState& State() const { return m_state; }
        const TankInput& Input() const;
        const TrackedDriverInput& DriverInput() const;
        const TankSettings& Settings() const;

    private:
        void SpawnAssaultRound();
        void AdvanceAssaultProjectiles(float deltaTimeSeconds);
        void UpdateEnemyGroups(float dt = 0);
        void AdvanceEnemyNavigation(float dt);
        void UpdateEnemyAttacks(float deltaTimeSeconds);
        void AdvanceEnemyProjectiles(float deltaTimeSeconds);
        void ApplyAssaultImpact(std::uint32_t hitBodyId, float damage);
        void SpawnMortarShell();
        void AdvanceMortarProjectiles(float deltaTimeSeconds);
        void ApplyMortarBlast(const Vec3& center, float radius, float damage);
        bool InitializeInternal(
            const TankSettings& settings,
            const PhysicsEnvironmentSettings& environmentSettings,
            const std::vector<MapPrimitive>& mapPrimitives,
            const MapSpawn& spawn,
            const std::vector<Map::HitTriangleMesh>* staticMeshes,
            bool createDefaultFloor,
            std::string* error);
        struct Impl;

        TrackedVehicleTestState m_state = {};
        std::unique_ptr<Impl> m_impl;
    };
}
