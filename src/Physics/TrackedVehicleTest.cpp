#include "TrackedVehicleTest.h"
#include "TankJoltShape.h"
#include "TankHitRegionQuery.h"
#include "PhysicsWorld.h"
#include "TankController.h"
#include "StaticMeshShape.h"
#include "SimpleNavigationMovement.h"
#include "EnemyTrackedSteering.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <optional>
#include <set>

JPH_SUPPRESS_WARNINGS

namespace Tank::Physics
{
    namespace Layers
    {
        constexpr JPH::ObjectLayer NonMoving = 0;
        constexpr JPH::ObjectLayer Moving = 1;
    }

    struct TrackedVehicleTest::Impl
    {
        PhysicsWorld world;
        TankController controller;
        JPH::BodyID floorBodyId;
        std::vector<JPH::BodyID> obstacleBodyIds;
        std::vector<JPH::BodyID> gateBodyIds;
        std::vector<JPH::BodyID> destructibleBodyIds;
        std::vector<JPH::BodyID> turretBodyIds;
        std::uint64_t nextCombatTargetId = 1;
        std::uint64_t resolvedAssaultRounds = 0;
        std::uint64_t resolvedMortarShots = 0;
        AssaultProjectileSettings projectileSettings = {};
        std::vector<EnemyAttackType> enemyAttackTypes;
        std::vector<EnemyGroupCommand> groupCommands;
        std::vector<std::optional<EnemyGroupBehavior>> groupBehaviors;
        std::vector<std::optional<EnemyNavigationPlanner>> groupNavigation;
        std::vector<std::string> groupAnchorSurfaces;
        std::vector<std::vector<size_t>> groupTurretIndices;
        struct NavigationDriver
        {
            EnemyNavigationDriver config;
            size_t enemyIndex = 0, groupIndex = 0;
            std::optional<SimpleNavigationMovement> movement;
            Vec3 target;
            std::string targetSurface;
            Vec3 routeStart;
            Vec3 navigationAnchor;
            std::unique_ptr<TankController> physical;
            size_t physicalNode = 0;
            float turnSign = 1;
        };
        std::vector<NavigationDriver> navigationDrivers;
        std::optional<EnemySimulationSettings> simulationSettings;
        double enemyObservationTimeSeconds = 0;
        PlayerCombatState playerCombat;
        float lostYaw = 0;
        bool requireNeutralInput = false;
        bool hasFloorBody = false;

        void DestroyCombatBody(JPH::BodyID& id)
        {
            for (auto& driver : navigationDrivers)
                if (driver.physical && driver.physical->IsTankBody(id.GetIndexAndSequenceNumber()))
                {
                    driver.physical.reset();
                    id = JPH::BodyID();
                    return;
                }
            auto& bodies = world.GetBodyInterface();
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
            id = JPH::BodyID();
        }

        ~Impl()
        {
            for (auto& driver : navigationDrivers)
                if (driver.physical)
                {
                    turretBodyIds[driver.enemyIndex] = JPH::BodyID();
                    driver.physical.reset();
                }
            JPH::BodyInterface& bodyInterface = world.GetBodyInterface();
            for (const JPH::BodyID id : destructibleBodyIds)
            {
                if (id.IsInvalid()) continue;
                bodyInterface.RemoveBody(id);
                bodyInterface.DestroyBody(id);
            }
            for (const JPH::BodyID id : turretBodyIds)
            {
                if (id.IsInvalid()) continue;
                bodyInterface.RemoveBody(id);
                bodyInterface.DestroyBody(id);
            }
            for (const JPH::BodyID obstacleBodyId : obstacleBodyIds)
            {
                bodyInterface.RemoveBody(obstacleBodyId);
                bodyInterface.DestroyBody(obstacleBodyId);
            }
            if (hasFloorBody)
            {
                bodyInterface.RemoveBody(floorBodyId);
                bodyInterface.DestroyBody(floorBodyId);
            }
        }
    };

    TrackedVehicleTest::TrackedVehicleTest() = default;
    TrackedVehicleTest::~TrackedVehicleTest() = default;

    void TrackedVehicleTest::Initialize()
    {
        Initialize({});
    }

    void TrackedVehicleTest::Initialize(const TankSettings& settings)
    {
        Initialize(settings, {});
    }

    void TrackedVehicleTest::Initialize(
        const TankSettings& settings,
        const PhysicsEnvironmentSettings& environmentSettings,
        const std::vector<MapPrimitive>& mapPrimitives,
        const MapSpawn& spawn)
    {
        InitializeInternal(settings, environmentSettings, mapPrimitives, spawn, nullptr, true, nullptr);
    }

    bool TrackedVehicleTest::InitializeWithStaticMeshes(
        const TankSettings& settings,
        const PhysicsEnvironmentSettings& environmentSettings,
        const std::vector<Map::HitTriangleMesh>& staticMeshes,
        const MapSpawn& spawn,
        std::string& error)
    {
        return InitializeInternal(settings, environmentSettings, {}, spawn, &staticMeshes, false, &error);
    }

    bool TrackedVehicleTest::InitializeInternal(
        const TankSettings& settings,
        const PhysicsEnvironmentSettings& environmentSettings,
        const std::vector<MapPrimitive>& mapPrimitives,
        const MapSpawn& spawn,
        const std::vector<Map::HitTriangleMesh>* staticMeshes,
        bool createDefaultFloor,
        std::string* error)
    {
        m_state = {};

        m_impl = std::make_unique<Impl>();
        m_impl->world.Initialize();
        std::string combatError;
        m_impl->playerCombat.Initialize({}, combatError);

        JPH::BodyInterface& bodyInterface = m_impl->world.GetBodyInterface();

        if (createDefaultFloor)
        {
            const float floorSizeM =
                std::clamp(environmentSettings.floorSizeM, 20.0f, 1000.0f);
            const float floorFriction =
                std::clamp(environmentSettings.floorFriction, 0.0f, 2.0f);
            const float floorHalfExtent = 0.5f * floorSizeM;
            JPH::BodyCreationSettings floorSettings(
                new JPH::BoxShape(JPH::Vec3(floorHalfExtent, 1.0f, floorHalfExtent)),
                JPH::RVec3(0.0, -1.0, 0.0),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                Layers::NonMoving);
            floorSettings.mFriction = floorFriction;

            JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
            m_impl->floorBodyId = floorBody->GetID();
            m_impl->hasFloorBody = true;
            bodyInterface.AddBody(m_impl->floorBodyId, JPH::EActivation::DontActivate);
        }

        m_impl->obstacleBodyIds.reserve(mapPrimitives.size());
        for (const MapPrimitive& primitive : mapPrimitives)
        {
            JPH::RefConst<JPH::Shape> shape;
            if (primitive.type == MapPrimitiveType::Box)
            {
                shape = new JPH::BoxShape(JPH::Vec3(
                    0.5f * primitive.size.x,
                    0.5f * primitive.size.y,
                    0.5f * primitive.size.z));
            }
            else if (primitive.type == MapPrimitiveType::TriangularPrism)
            {
                const float halfX = 0.5f * primitive.size.x;
                const float halfY = 0.5f * primitive.size.y;
                const float halfZ = 0.5f * primitive.size.z;
                JPH::Array<JPH::Vec3> points = {
                    { -halfX, -halfY, -halfZ },
                    { halfX, -halfY, -halfZ },
                    { -halfX, -halfY, halfZ },
                    { halfX, -halfY, halfZ },
                    { -halfX, halfY, halfZ },
                    { halfX, halfY, halfZ } };
                JPH::ShapeSettings::ShapeResult shapeResult =
                    JPH::ConvexHullShapeSettings(points).Create();
                if (shapeResult.HasError())
                {
                    continue;
                }
                shape = shapeResult.Get();
            }
            else
            {
                const uint32_t sampleCount = primitive.heightFieldSampleCount;
                if (sampleCount < 2 ||
                    primitive.heightFieldHeights.size() !=
                        static_cast<size_t>(sampleCount) * sampleCount)
                {
                    continue;
                }
                const float halfSpan =
                    0.5f * primitive.heightFieldCellSizeM * static_cast<float>(sampleCount - 1);
                JPH::ShapeSettings::ShapeResult shapeResult =
                    JPH::HeightFieldShapeSettings(
                        primitive.heightFieldHeights.data(),
                        JPH::Vec3(-halfSpan, 0.0f, -halfSpan),
                        JPH::Vec3(
                            primitive.heightFieldCellSizeM,
                            1.0f,
                            primitive.heightFieldCellSizeM),
                        sampleCount).Create();
                if (shapeResult.HasError())
                {
                    continue;
                }
                shape = shapeResult.Get();
            }
            JPH::BodyCreationSettings obstacleSettings(
                shape,
                JPH::RVec3(
                    primitive.position.x,
                    primitive.position.y,
                    primitive.position.z),
                JPH::Quat::sRotation(JPH::Vec3::sAxisY(), primitive.yawRadians),
                JPH::EMotionType::Static,
                Layers::NonMoving);
            obstacleSettings.mFriction = std::clamp(primitive.friction, 0.0f, 2.0f);

            JPH::Body* obstacleBody = bodyInterface.CreateBody(obstacleSettings);
            if (obstacleBody != nullptr)
            {
                const JPH::BodyID obstacleBodyId = obstacleBody->GetID();
                m_impl->obstacleBodyIds.push_back(obstacleBodyId);
                bodyInterface.AddBody(obstacleBodyId, JPH::EActivation::DontActivate);
            }
        }

        if (staticMeshes != nullptr)
        {
            m_impl->obstacleBodyIds.reserve(m_impl->obstacleBodyIds.size() + staticMeshes->size());
            for (const Map::HitTriangleMesh& mesh : *staticMeshes)
            {
                if (mesh.breakable && (!std::isfinite(mesh.hitPoints) || mesh.hitPoints <= 0))
                { if (error) *error = "Invalid destructible mesh HP"; return false; }
                JPH::BodyID bodyId;
                std::string shapeError;
                if (mesh.gate)
                {
                    const auto& g = *mesh.gate;
                    const auto finiteVector = [](const auto& vector) {return std::all_of(vector.begin(), vector.end(),
                        [](float value) {return std::isfinite(value);});};
                    double travelSquared = 0;
                    for (float value : mesh.gateTravel) travelSquared += double(value) * value;
                    if (!finiteVector(mesh.gateTriggerCenter) || !finiteVector(mesh.gateTravel) || travelSquared <= 0 ||
                        mesh.breakable || !std::isfinite(g.triggerRadiusMeters) || g.triggerRadiusMeters <= 0 ||
                        !std::isfinite(g.openingSpeedMetersPerSecond) || g.openingSpeedMetersPerSecond <= 0)
                    { if (error) *error = "Invalid gate settings"; return false; }
                    JPH::Array<JPH::Vec3> points;
                    for (const auto& p : mesh.vertices) points.emplace_back(p[0], p[1], p[2]);
                    const auto shape = JPH::ConvexHullShapeSettings(points).Create();
                    if (shape.HasError()) { if (error) *error = shape.GetError().c_str(); return false; }
                    JPH::BodyCreationSettings gateBody(shape.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                        JPH::EMotionType::Kinematic, Layers::Moving);
                    bodyId = bodyInterface.CreateAndAddBody(gateBody, JPH::EActivation::DontActivate);
                    if (bodyId.IsInvalid()) { if (error) *error = "Cannot create gate collision"; return false; }
                    m_impl->obstacleBodyIds.push_back(bodyId);
                    m_impl->gateBodyIds.push_back(bodyId);
                    m_state.gates.push_back({mesh.placementId, mesh.gateTriggerCenter, mesh.gateTravel,
                        g.triggerRadiusMeters, g.openingSpeedMetersPerSecond});
                    continue;
                }
                if (!AddStaticMeshBody(m_impl->world, mesh, bodyId, shapeError))
                {
                    if (error != nullptr) *error = shapeError;
                    return false;
                }
                if (mesh.breakable)
                {
                    auto minimum = mesh.vertices.front(), maximum = minimum;
                    for (const auto& vertex : mesh.vertices)
                        for (size_t axis = 0; axis < 3; ++axis)
                        { minimum[axis] = (std::min)(minimum[axis], vertex[axis]); maximum[axis] = (std::max)(maximum[axis], vertex[axis]); }
                    const JPH::Vec3 center((minimum[0]+maximum[0])*0.5f, (minimum[1]+maximum[1])*0.5f, (minimum[2]+maximum[2])*0.5f);
                    const JPH::Vec3 size(maximum[0]-minimum[0], maximum[1]-minimum[1], maximum[2]-minimum[2]);
                    m_impl->destructibleBodyIds.push_back(bodyId);
                    m_state.destructibleBoxes.push_back({
                        {m_impl->nextCombatTargetId++, CombatTargetKind::Destructible, mesh.hitPoints, true},
                        {center.GetX(), center.GetY(), center.GetZ()}, {size.GetX(), size.GetY(), size.GetZ()}, mesh.placementId});
                }
                else m_impl->obstacleBodyIds.push_back(bodyId);
            }
        }

        m_impl->controller.Initialize(m_impl->world, settings, spawn);
        m_state.bodyPosition = spawn.position;
        m_state.bodyRotation = {0, std::sin(spawn.yawRadians*0.5f), 0, std::cos(spawn.yawRadians*0.5f)};
        SetAssaultProjectileSettings(settings.assaultProjectiles);
        if (error != nullptr) error->clear();
        return true;
    }

    const TankSettings& TrackedVehicleTest::Settings() const
    {
        static const TankSettings defaultSettings;
        return m_impl != nullptr ? m_impl->controller.Settings() : defaultSettings;
    }

    const TankInput& TrackedVehicleTest::Input() const
    {
        static const TankInput defaultInput;
        return m_impl != nullptr ? m_impl->controller.Input() : defaultInput;
    }

    const TrackedDriverInput& TrackedVehicleTest::DriverInput() const
    {
        static const TrackedDriverInput defaultInput;
        return m_impl != nullptr ? m_impl->controller.DriverInput() : defaultInput;
    }

    void TrackedVehicleTest::SetInput(const TankInput& input)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        if (m_impl->requireNeutralInput)
        {
            const bool neutral = std::abs(input.throttle)<0.001f && std::abs(input.steering)<0.001f &&
                std::abs(input.roll)<0.001f && std::abs(input.leftLeverX)<0.001f && std::abs(input.rightLeverX)<0.001f && !input.fireAssault;
            if (!neutral) { m_impl->controller.SetInput({}); return; }
            m_impl->requireNeutralInput = false;
        }
        m_impl->controller.SetInput(m_impl->playerCombat.Snapshot().phase == PlayerCombatPhase::Alive ? input : TankInput{});
    }

    bool TrackedVehicleTest::ApplyConfiguredRecoil()
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        return m_impl->playerCombat.Snapshot().phase == PlayerCombatPhase::Alive && m_impl->controller.ApplyConfiguredRecoil();
    }

    bool TrackedVehicleTest::FireAssault()
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        if (m_impl->requireNeutralInput || m_impl->playerCombat.Snapshot().phase != PlayerCombatPhase::Alive) return false;
        m_impl->controller.SetAssaultCapacityAvailable(
            m_state.assaultProjectiles.size() < static_cast<size_t>(m_impl->projectileSettings.maximumCount));
        if (!m_impl->controller.FireAssault()) return false;
        SpawnAssaultRound();
        return true;
    }

    const AssaultProjectileSettings& TrackedVehicleTest::ProjectileSettings() const
    {
        static const AssaultProjectileSettings defaults;
        return m_impl ? m_impl->projectileSettings : defaults;
    }

    void TrackedVehicleTest::SetAssaultProjectileSettings(const AssaultProjectileSettings& settings)
    {
        if (!m_impl) return;
        auto& current = m_impl->projectileSettings;
        current.maximumCount = std::clamp(settings.maximumCount, 0, 1024);
        current.expireAtMaximumDistance = settings.expireAtMaximumDistance;
        current.maximumDistanceMeters = std::isfinite(settings.maximumDistanceMeters)
            ? std::clamp(settings.maximumDistanceMeters, 0.1f, 1000000.0f) : 40.0f;
        current.muzzleLocalPosition = {
            std::isfinite(settings.muzzleLocalPosition.x)
                ? std::clamp(settings.muzzleLocalPosition.x, -100.0f, 100.0f) : 0.0f,
            std::isfinite(settings.muzzleLocalPosition.y)
                ? std::clamp(settings.muzzleLocalPosition.y, -100.0f, 100.0f) : 0.85f,
            std::isfinite(settings.muzzleLocalPosition.z)
                ? std::clamp(settings.muzzleLocalPosition.z, -100.0f, 100.0f) : 1.8f};
        current.maximumImpactMarks = std::clamp(settings.maximumImpactMarks, 0, 1024);
        m_state.assaultImpactMarks.SetCapacity(current.maximumImpactMarks);
        current.speedMetersPerSecond = std::isfinite(settings.speedMetersPerSecond)
            ? std::clamp(settings.speedMetersPerSecond, 0.1f, 10000.0f) : 80.0f;
        current.damagePerRound = std::isfinite(settings.damagePerRound)
            ? std::clamp(settings.damagePerRound, 0.0f, 1000000.0f) : 20.0f;
        current.lifetimeSeconds = std::isfinite(settings.lifetimeSeconds)
            ? std::clamp(settings.lifetimeSeconds, 0.0f, 86400.0f) : 0.0f;
        auto& projectiles = m_state.assaultProjectiles;
        if (projectiles.size() > static_cast<size_t>(current.maximumCount))
            projectiles.erase(projectiles.begin(), projectiles.end() - current.maximumCount);
        m_impl->controller.SetAssaultCapacityAvailable(
            projectiles.size() < static_cast<size_t>(current.maximumCount));
    }

    bool TrackedVehicleTest::SetEnemySupport(const std::string& enemyPlacementId, const std::string& supportPlacementId)
    {
        const auto support = std::find_if(m_state.destructibleBoxes.begin(), m_state.destructibleBoxes.end(),
            [&](const auto& object) {return !supportPlacementId.empty() && object.placementId == supportPlacementId;});
        if (support == m_state.destructibleBoxes.end()) return false;
        for (auto& enemy : m_state.fixedTurrets)
            if (enemy.placementId == enemyPlacementId)
            { enemy.supportPlacementId = supportPlacementId; return true; }
        return false;
    }

    bool TrackedVehicleTest::AddDestructibleBox(const Vec3& position, const Vec3& size, float hitPoints)
    {
        if (!m_impl || !std::isfinite(hitPoints) || hitPoints <= 0.0f ||
            !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
            !std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
            size.x < 0.1f || size.y < 0.1f || size.z < 0.1f) return false;
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(JPH::Vec3(size.x, size.y, size.z) * 0.5f),
            JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, Layers::NonMoving);
        auto& bodies = m_impl->world.GetBodyInterface();
        JPH::Body* body = bodies.CreateBody(settings);
        if (!body) return false;
        const auto id = body->GetID();
        bodies.AddBody(id, JPH::EActivation::DontActivate);
        m_impl->destructibleBodyIds.push_back(id);
        m_state.destructibleBoxes.push_back({
            {m_impl->nextCombatTargetId++, CombatTargetKind::Destructible, hitPoints, true},
            position, size});
        return true;
    }

    bool TrackedVehicleTest::AddFixedTurret(const Vec3& position, const Vec3& size, float hitPoints, const Quat& rotation,
        const std::string& placementId, const EnemyUnitType& unitType)
    {
        if (m_impl && !m_impl->enemyAttackTypes.empty())
            for (const auto& mount : unitType.attackMounts)
                if (mount.attackTypeIndex < 0 || static_cast<size_t>(mount.attackTypeIndex) >= m_impl->enemyAttackTypes.size()) return false;
        const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w;
        if (!std::isfinite(lengthSquared) || std::abs(lengthSquared - 1.0f) > 0.001f) return false;

        if (!m_impl || !std::isfinite(hitPoints) || hitPoints <= 0.0f ||
            !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
            !std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
            size.x < 0.1f || size.y < 0.1f || size.z < 0.1f) return false;
        EnemyUnitType resolvedUnitType = unitType;
        const auto defaultProfile = MakeTankCollisionProfile({1.0f, 1.0f, 1.0f});
        if (resolvedUnitType.collisionProfile == defaultProfile &&
            (size.x != 1.0f || size.y != 1.0f || size.z != 1.0f))
        {
            resolvedUnitType.collisionProfile = MakeTankCollisionProfile(size);
        }
        const auto movementShape =
            CreateTankJoltShape(resolvedUnitType.collisionProfile.movementShape);
        if (movementShape == nullptr) return false;
        JPH::BodyCreationSettings settings(
            movementShape,
            JPH::RVec3(position.x, position.y, position.z), JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
            JPH::EMotionType::Static, Layers::NonMoving);
        auto& bodies = m_impl->world.GetBodyInterface();
        settings.mAllowDynamicOrKinematic = true;
        JPH::Body* body = bodies.CreateBody(settings);
        if (!body) return false;
        const auto id = body->GetID();
        bodies.AddBody(id, JPH::EActivation::DontActivate);
        m_impl->turretBodyIds.push_back(id);
        m_state.fixedTurrets.push_back({
            {m_impl->nextCombatTargetId++, CombatTargetKind::Enemy, hitPoints, true},
            position,
            resolvedUnitType.collisionProfile.movementShape.sizeMeters,
            rotation, placementId, resolvedUnitType, {}});
        auto& turret = m_state.fixedTurrets.back();
        const auto forward = JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w) * JPH::Vec3::sAxisZ();
        if (!m_impl->enemyAttackTypes.empty())
            for (const auto& mount : unitType.attackMounts)
            {
                const auto& type = m_impl->enemyAttackTypes[static_cast<size_t>(mount.attackTypeIndex)];
                const auto offset = JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w) *
                    JPH::Vec3(mount.localPosition.x, mount.localPosition.y, mount.localPosition.z);
                turret.mounts.push_back({type, mount.localPosition,
                    {position.x+offset.GetX(), position.y+offset.GetY(), position.z+offset.GetZ()},
                    {std::atan2(forward.GetX(), forward.GetZ()), type.firingIntervalSeconds}});
                turret.mounts.back().aim.initialYawRadians = turret.mounts.back().aim.yawRadians;
            }
        return true;
    }

    void TrackedVehicleTest::RequireNeutralInput()
    {
        if (m_impl) m_impl->requireNeutralInput = true;
    }

    bool TrackedVehicleTest::ConfigureEnemyAttacks(const std::vector<EnemyAttackType>& types)
    {
        if (!m_impl || !m_state.fixedTurrets.empty() || types.empty() ||
            std::any_of(types.begin(), types.end(), [](const auto& type) { return !IsValidEnemyAttackType(type); })) return false;
        m_impl->enemyAttackTypes = types;
        return true;
    }

    bool TrackedVehicleTest::ConfigureEnemyGroups(const std::vector<Map::EnemyGroup>& groups)
    {
        if (!m_impl || !m_impl->navigationDrivers.empty()) return false;
        std::vector<EnemyGroupState> states;
        std::vector<std::optional<EnemyGroupBehavior>> behaviors;
        std::vector<std::vector<size_t>> indices;
        std::set<std::string> groupIds, assignedEnemies;
        for (const auto& group : groups)
        {
            if (group.id.empty() || !groupIds.insert(group.id).second || group.members.empty()) return false;
            std::optional<EnemyGroupBehavior> behavior;
            if (group.patrol)
            {
                behavior.emplace();
                if (!behavior->Initialize(*group.patrol)) return false;
            }
            EnemyGroupState state;
            state.id = group.id;
            if (behavior) state.behavior = behavior->State();
            behaviors.push_back(std::move(behavior));
            std::vector<size_t> groupIndices;
            std::set<int> priorities;
            for (const auto& member : group.members)
            {
                if (member.enemyId.empty() || member.commandPriority < 0 ||
                    !priorities.insert(member.commandPriority).second ||
                    !assignedEnemies.insert(member.enemyId).second) return false;
                const auto& turrets = m_state.fixedTurrets;
                const auto matches = [&](const auto& turret) { return turret.placementId == member.enemyId; };
                if (std::count_if(turrets.begin(), turrets.end(), matches) != 1) return false;
                const auto turret = std::find_if(turrets.begin(), turrets.end(), matches);
                groupIndices.push_back(static_cast<size_t>(turret - turrets.begin()));
                state.members.push_back({member.enemyId, member.commandPriority, turret->target.active});
            }
            states.push_back(std::move(state));
            indices.push_back(std::move(groupIndices));
        }
        m_impl->groupNavigation = std::vector<std::optional<EnemyNavigationPlanner>>(groups.size());
        m_impl->groupAnchorSurfaces = std::vector<std::string>(groups.size());
        m_impl->groupBehaviors = std::move(behaviors);
        m_state.enemyGroups = std::move(states);
        m_impl->groupTurretIndices = std::move(indices);
        m_impl->groupCommands = std::vector<EnemyGroupCommand>(groups.size());
        UpdateEnemyGroups();
        return true;
    }

    bool TrackedVehicleTest::ConfigureEnemyMovement(const EnemyNavigationSetup& setup, std::string& error)
    {
        if (!m_impl || !m_impl->navigationDrivers.empty()) { error = "Enemy movement already configured or uninitialized"; return false; }
        if (!ValidateNavigationGrid(setup.grid, error)) return false;
        std::vector<Impl::NavigationDriver> drivers;
        std::set<size_t> groups, enemies;
        for (const auto& config : setup.drivers)
        {
            const auto group = std::find_if(m_state.enemyGroups.begin(), m_state.enemyGroups.end(),
                [&](const auto& value) { return value.id == config.groupId; });
            const auto enemy = std::find_if(m_state.fixedTurrets.begin(), m_state.fixedTurrets.end(),
                [&](const auto& value) { return value.placementId == config.enemyId; });
            if (group == m_state.enemyGroups.end() || enemy == m_state.fixedTurrets.end() || !group->behavior ||
                !enemy->target.active || std::none_of(group->members.begin(), group->members.end(),
                    [&](const auto& member) { return member.enemyId == config.enemyId; }))
            { error = "Moving enemy must belong to an active patrol group"; return false; }
            const size_t groupIndex = static_cast<size_t>(group - m_state.enemyGroups.begin());
            const size_t enemyIndex = static_cast<size_t>(enemy - m_state.fixedTurrets.begin());
            EnemyNavigationPlanner planner;
            if (!groups.insert(groupIndex).second || !enemies.insert(enemyIndex).second ||
                !planner.Configure(setup.grid, std::span(&config.mobility, 1), config.snapDistanceMeters))
            { error = "Invalid movement settings or multiple moving enemies per group"; return false; }
            EnemyGroupBehaviorSnapshot request;
            request.hasMoveTarget = true; request.moveTarget = enemy->position; request.targetSurfaceId = config.surfaceId;
            planner.Update(request, enemy->position, config.surfaceId);
            if (planner.State().stopped) { error = planner.State().error; return false; }
            drivers.push_back({config, enemyIndex, groupIndex, {}, {}, {}});
            drivers.back().navigationAnchor = planner.State().path->cells.front().position;
        }
        for (const auto& driver : drivers)
        {
            ConfigureGroupNavigation(driver.config.groupId, setup.grid, std::span(&driver.config.mobility, 1),
                driver.config.snapDistanceMeters, driver.config.surfaceId);
            auto& enemy = m_state.fixedTurrets[driver.enemyIndex];
            enemy.simpleMoving = true;
            auto& bodies = m_impl->world.GetBodyInterface();
            bodies.SetMotionType(m_impl->turretBodyIds[driver.enemyIndex], JPH::EMotionType::Kinematic, JPH::EActivation::Activate);
            bodies.SetObjectLayer(m_impl->turretBodyIds[driver.enemyIndex], Layers::Moving);
        }
        m_impl->navigationDrivers = std::move(drivers);
        error.clear();
        return true;
    }

    bool TrackedVehicleTest::SetEnemySimulationSettings(const EnemySimulationSettings& settings)
    {
        if (!m_impl || !IsValidEnemySimulationSettings(settings)) return false;
        m_impl->simulationSettings = settings;
        return true;
    }

    void TrackedVehicleTest::AdvanceEnemyNavigation(float dt)
    {
        for (auto& driver : m_impl->navigationDrivers)
        {
            auto& enemy = m_state.fixedTurrets[driver.enemyIndex];
            if (!enemy.target.active) { enemy.physicalMoving = false; enemy.runningGear.reset(); continue; }
            if (enemy.supportReleased)
            {
                if (driver.physical) {driver.physical->SetInput({}); driver.physical->PreStep();}
                continue;
            }
            if (m_impl->simulationSettings && !m_impl->simulationSettings->movementEnabled)
            {
                auto& bodies = m_impl->world.GetBodyInterface();
                auto& bodyId = m_impl->turretBodyIds[driver.enemyIndex];
                if (driver.physical)
                {
                    driver.physical.reset();
                    JPH::BodyCreationSettings proxy(CreateTankJoltShape(enemy.unitType.collisionProfile.movementShape),
                        JPH::RVec3(enemy.position.x, enemy.position.y, enemy.position.z),
                        JPH::Quat(enemy.rotation.x, enemy.rotation.y, enemy.rotation.z, enemy.rotation.w),
                        JPH::EMotionType::Kinematic, Layers::Moving);
                    auto* body = bodies.CreateBody(proxy);
                    bodyId = body->GetID();
                    bodies.AddBody(bodyId, JPH::EActivation::Activate);
                    driver.movement.reset(); // Replan from the stopped pose when movement resumes.
                }
                bodies.SetLinearAndAngularVelocity(bodyId, JPH::Vec3::sZero(), JPH::Vec3::sZero());
                enemy.simpleMoving = false;
                enemy.physicalMoving = false;
                enemy.runningGear.reset();
                enemy.navigationVelocity = {};
                enemy.navigationYawRadiansPerSecond = 0;
                continue;
            }
            enemy.simpleMoving = true;
            const auto player = m_impl->controller.State().body.position;
            const auto distance = NavigationDistance(player, enemy.position);
            auto& bodyId = m_impl->turretBodyIds[driver.enemyIndex];
            auto& bodies = m_impl->world.GetBodyInterface();
            const auto& group = m_state.enemyGroups[driver.groupIndex];
            if (m_impl->simulationSettings && !driver.physical &&
                distance <= m_impl->simulationSettings->physicalEnterMeters)
            {
                bodies.RemoveBody(bodyId);
                bodies.DestroyBody(bodyId);
                bodyId = JPH::BodyID();
                driver.physical = std::make_unique<TankController>();
                auto settings = m_impl->controller.Settings();
                settings.startUpsideDown = false;
                settings.rollingInputEnabled = false;
                settings.collisionProfile = enemy.unitType.collisionProfile;
                enemy.size = settings.collisionProfile.movementShape.sizeMeters;
                driver.physical->Initialize(m_impl->world, settings, {enemy.position, 0});
                driver.physical->RestoreMotion({enemy.position, enemy.rotation}, enemy.navigationVelocity);
                bodyId = JPH::BodyID(driver.physical->BodyId());
                driver.physicalNode = driver.movement ? driver.movement->State().segmentIndex + 1 : 0;
                enemy.physicalMoving = true;
            }
            if (driver.physical)
            {
                // Return only when a known surface node is reached upright; otherwise retain physics.
                if (m_impl->simulationSettings && distance >= m_impl->simulationSettings->simpleReturnMeters &&
                    std::abs(enemy.navigationVelocity.y) < 0.1f &&
                    std::any_of(driver.physical->State().wheels.begin(), driver.physical->State().wheels.end(),
                        [](const auto& wheel) { return wheel.hasContact; }) &&
                    std::abs(enemy.rotation.x) < 0.1f && std::abs(enemy.rotation.z) < 0.1f &&
                    group.navigation && group.navigation->path)
                {
                    const auto& cells = group.navigation->path->cells;
                    auto cell = std::find_if(cells.begin(), cells.end(), [&](const auto& value)
                        { return NavigationDistance(value.position, enemy.position) <= driver.config.snapDistanceMeters; });
                    if (cell != cells.end())
                    {
                        m_impl->groupAnchorSurfaces[driver.groupIndex] = cell->surfaceId;
                        driver.navigationAnchor = cell->position;
                        driver.physical.reset();
                        JPH::BodyCreationSettings proxy(CreateTankJoltShape(
                                enemy.unitType.collisionProfile.movementShape),
                            JPH::RVec3(enemy.position.x, enemy.position.y, enemy.position.z),
                            JPH::Quat(enemy.rotation.x, enemy.rotation.y, enemy.rotation.z, enemy.rotation.w),
                            JPH::EMotionType::Kinematic, Layers::Moving);
                        auto* body = bodies.CreateBody(proxy);
                        bodyId = body->GetID();
                        bodies.AddBody(bodyId, JPH::EActivation::Activate);
                        enemy.physicalMoving = false;
                        enemy.runningGear.reset();
                        driver.movement.reset(); // Replan from actual physical position after this step.
                    }
                }
                if (driver.physical)
                {
                    const auto& tuning = *m_impl->simulationSettings;
                    const float speed = std::hypot(enemy.navigationVelocity.x, enemy.navigationVelocity.z);
                    TankInput input;
                    input.brake = true;
                    if (group.navigation && !group.navigation->stopped && group.navigation->path &&
                        group.behavior && group.behavior->hasMoveTarget)
                    {
                        const auto& cells = group.navigation->path->cells;
                        while (speed < 0.25f && driver.physicalNode < cells.size() &&
                            std::hypot(cells[driver.physicalNode].position.x-enemy.position.x,
                                cells[driver.physicalNode].position.z-enemy.position.z) <= tuning.nodeArrivalMeters)
                        {
                            m_impl->groupAnchorSurfaces[driver.groupIndex] = cells[driver.physicalNode].surfaceId;
                            driver.navigationAnchor = driver.physicalNode == 0 ? driver.routeStart : cells[driver.physicalNode].position;
                            ++driver.physicalNode;
                        }
                        if (driver.physicalNode < cells.size())
                        {
                            const auto target = cells[driver.physicalNode].position;
                            input = EnemyTrackedSteering(driver.physical->State(), target,
                                driver.config.mobility.speedMetersPerSecond, tuning.brakingDecelerationMetersPerSecondSquared,
                                tuning.nodeArrivalMeters, driver.turnSign);
                            if (input.leftTrack * input.rightTrack < 0) driver.turnSign = -input.leftTrack;
                        }
                    }
                    driver.physical->SetInput(input);
                    driver.physical->PreStep();
                    continue;
                }
            }
            const auto previous = enemy.position;
            enemy.navigationYawRadiansPerSecond = 0;
            if (driver.movement && group.navigation && !group.navigation->stopped)
            {
                const auto& current = driver.movement->State();
                bool aligned = true;
                if (!current.arrived)
                {
                    const auto next = group.navigation->path->cells[current.segmentIndex+1].position;
                    const float dx = next.x-enemy.position.x, dz = next.z-enemy.position.z;
                    if (std::hypot(dx, dz) > 0.00001f)
                    {
                        const auto& q = enemy.rotation;
                        const float yaw = std::atan2(2*(q.w*q.y+q.x*q.z), 1-2*(q.y*q.y+q.z*q.z));
                        float error = std::remainder(std::atan2(dx, dz)-yaw, 6.283185307f);
                        if (std::abs(error) > 3.14f) error = std::copysign(std::abs(error), driver.turnSign);
                        const float rate = m_impl->simulationSettings.value_or(EnemySimulationSettings{}).simpleTurnDegreesPerSecond *
                            3.141592653589793f/180;
                        const float turn = std::clamp(error, -rate*dt, rate*dt);
                        enemy.navigationYawRadiansPerSecond = turn/dt;
                        if (std::abs(turn) > 0.00001f) driver.turnSign = std::copysign(1.0f, turn);
                        enemy.rotation = {0, std::sin((yaw+turn)*0.5f), 0, std::cos((yaw+turn)*0.5f)};
                        aligned = std::abs(error) < 0.0001f;
                    }
                }
                if (aligned) driver.movement->Advance(dt, true);
                const auto& movement = driver.movement->State();
                enemy.position = movement.position;
                m_impl->groupAnchorSurfaces[driver.groupIndex] = movement.surfaceId;
                driver.navigationAnchor = movement.segmentIndex == 0 ? driver.routeStart :
                    group.navigation->path->cells[movement.segmentIndex].position;

            }
            enemy.navigationVelocity = {(enemy.position.x-previous.x)/dt, (enemy.position.y-previous.y)/dt,
                (enemy.position.z-previous.z)/dt};
            const auto& p = enemy.position; const auto& q = enemy.rotation;
            m_impl->world.GetBodyInterface().MoveKinematic(m_impl->turretBodyIds[driver.enemyIndex],
                JPH::RVec3(p.x,p.y,p.z), JPH::Quat(q.x,q.y,q.z,q.w), dt);
        }
    }

    bool TrackedVehicleTest::ConfigureGroupNavigation(const std::string& groupId, const NavigationGrid& grid,
        std::span<const NavigationMobility> movingMembers, double snapDistance, const std::string& anchorSurface)
    {
        if (!m_impl || anchorSurface.empty() || std::none_of(grid.cells.begin(), grid.cells.end(),
            [&](const auto& cell) { return cell.surfaceId == anchorSurface; })) return false;
        const auto found = std::find_if(m_state.enemyGroups.begin(), m_state.enemyGroups.end(),
            [&](const auto& group) { return group.id == groupId; });
        if (found == m_state.enemyGroups.end() || !found->behavior) return false;
        EnemyNavigationPlanner planner;
        if (!planner.Configure(grid, movingMembers, snapDistance)) return false;
        const size_t index = static_cast<size_t>(found - m_state.enemyGroups.begin());
        if (std::any_of(m_impl->navigationDrivers.begin(), m_impl->navigationDrivers.end(),
            [&](const auto& driver) { return driver.groupIndex == index; })) return false;
        m_impl->groupNavigation[index] = std::move(planner);
        m_impl->groupAnchorSurfaces[index] = anchorSurface;
        found->navigation = m_impl->groupNavigation[index]->State();
        return true;
    }

    void TrackedVehicleTest::UpdateEnemyGroups(float dt)
    {
        const bool playerAlive = m_impl->playerCombat.Snapshot().phase == PlayerCombatPhase::Alive;
        const bool movementEnabled = !m_impl->simulationSettings || m_impl->simulationSettings->movementEnabled;
        for (size_t i = 0; i < m_state.enemyGroups.size(); ++i)
        {
            auto& group = m_state.enemyGroups[i];
            group.detected = false;
            for (size_t j = 0; j < group.members.size(); ++j)
            {
                const auto& turret = m_state.fixedTurrets[m_impl->groupTurretIndices[i][j]];
                group.members[j].canCommand = turret.target.active;
                if (playerAlive && turret.target.active && std::any_of(turret.mounts.begin(), turret.mounts.end(),
                    [](const auto& mount) { return mount.aim.detected; }))
                    group.detected = true;
            }
            m_impl->groupCommands[i].Update(group.members);
            group.leaderId = m_impl->groupCommands[i].LeaderId();
            if (group.detected)
            {
                // All detections in this step observe the same player, so no observer arbitration is needed.
                group.hasLastKnownPosition = true;
                group.lastKnownPosition = m_impl->controller.State().body.position;
                group.lastObservedSeconds = m_impl->enemyObservationTimeSeconds;
                group.lastNavigationObservation.reset();
                if (movementEnabled && m_impl->groupNavigation[i])
                {
                    const auto& state = m_impl->controller.State();
                    const TrackedWheelState* support = nullptr;
                    for (int w = 0; w < state.wheelCount; ++w)
                    {
                        const auto& wheel = state.wheels[w];
                        if (wheel.hasContact && wheel.contactNormal.y > 0.5f &&
                            (!support || wheel.suspensionImpulseNewtonSeconds > support->suspensionImpulseNewtonSeconds)) support = &wheel;
                    }
                    if (support)
                    {
                        const auto tuning = m_impl->simulationSettings.value_or(EnemySimulationSettings{});
                        group.lastNavigationObservation = m_impl->groupNavigation[i]->ResolveObservation(
                            group.lastKnownPosition, support->contactPosition,
                            tuning.observationRadiusMeters, tuning.supportHeightToleranceMeters);
                    }
                }
            }
            if (movementEnabled && dt > 0 && m_impl->groupBehaviors[i])
            {
                Vec3 anchor;
                for (size_t j = 0; j < group.members.size(); ++j)
                    if (group.members[j].enemyId == group.leaderId)
                        anchor = m_state.fixedTurrets[m_impl->groupTurretIndices[i][j]].position;
                auto driver = std::find_if(m_impl->navigationDrivers.begin(), m_impl->navigationDrivers.end(),
                    [&](const auto& value) { return value.groupIndex == i; });
                if (driver != m_impl->navigationDrivers.end())
                {
                    anchor = m_state.fixedTurrets[driver->enemyIndex].position;
                    if (driver->physical && !group.detected && group.navigation && group.navigation->path &&
                        !group.navigation->path->cells.empty())
                    {
                        const auto node = std::min(driver->physicalNode > 0 ? driver->physicalNode-1 : 0,
                            group.navigation->path->cells.size()-1);
                        anchor = node == 0 ? driver->routeStart : group.navigation->path->cells[node].position;
                    }
                    // Patrol arrival is evaluated at reached nodes, not mid-link, to avoid truncating a route.
                    if (!driver->physical && driver->movement && !driver->movement->State().arrived && !group.detected && group.navigation && group.navigation->path)
                        anchor = driver->movement->State().segmentIndex == 0 ? driver->routeStart :
                            group.navigation->path->cells[driver->movement->State().segmentIndex].position;
                }
                m_impl->groupBehaviors[i]->Update(dt, playerAlive && !group.leaderId.empty() &&
                    (driver == m_impl->navigationDrivers.end() || m_state.fixedTurrets[driver->enemyIndex].target.active),
                    anchor, group.detected, group.lastKnownPosition,
                    group.lastNavigationObservation ? group.lastNavigationObservation->surfaceId : std::string{},
                    group.lastNavigationObservation ? &group.lastNavigationObservation->position : nullptr);
                group.behavior = m_impl->groupBehaviors[i]->State();
                if (movementEnabled && m_impl->groupNavigation[i])
                {
                    const auto& request = *group.behavior;
                    const bool sameTarget = driver != m_impl->navigationDrivers.end() && driver->movement &&
                        driver->target.x == request.moveTarget.x && driver->target.y == request.moveTarget.y &&
                        driver->target.z == request.moveTarget.z && driver->targetSurface == request.targetSurfaceId;
                    if (!sameTarget || !request.hasMoveTarget)
                    {
                        if (driver != m_impl->navigationDrivers.end()) anchor = driver->navigationAnchor;
                        m_impl->groupNavigation[i]->Update(request, anchor, m_impl->groupAnchorSurfaces[i]);
                        group.navigation = m_impl->groupNavigation[i]->State();
                        if (driver != m_impl->navigationDrivers.end())
                        {
                            driver->movement.reset();
                            if (group.navigation->path && !group.navigation->stopped)
                            {
                                auto path = *group.navigation->path;
                                driver->routeStart = path.cells.front().position;
                                path.cells.front().position = m_state.fixedTurrets[driver->enemyIndex].position;
                                SimpleNavigationMovement movement;
                                if (movement.Initialize(path, driver->config.mobility))
                                {
                                    driver->movement = std::move(movement);
                                    driver->physicalNode = path.cells.size() > 1 ? 1 : 0;
                                    driver->target = request.moveTarget;
                                    driver->targetSurface = request.targetSurfaceId;
                                    group.navigation->path = std::move(path);
                                }
                                else
                                { group.navigation->stopped = true; group.navigation->path.reset(); group.navigation->error = "Cannot initialize movement from current position"; }
                            }
                        }
                    }
                }
            }
        }
    }

    void TrackedVehicleTest::SpawnAssaultRound()
    {
        m_state.assaultWeapon = m_impl->controller.State().assaultWeapon;
        m_impl->resolvedAssaultRounds = m_state.assaultWeapon.roundsFired;
        const auto& settings = m_impl->projectileSettings;
        const auto forward = m_impl->controller.AssaultForwardDirection();
        m_state.assaultProjectiles.push_back({m_impl->controller.AssaultMuzzlePosition(
            settings.muzzleLocalPosition),
            {forward.x * settings.speedMetersPerSecond,
             forward.y * settings.speedMetersPerSecond,
             forward.z * settings.speedMetersPerSecond},
            settings.damagePerRound, 0.0f, settings.lifetimeSeconds, 0.0f,
            settings.expireAtMaximumDistance, settings.maximumDistanceMeters});
    }

    void TrackedVehicleTest::AdvanceAssaultProjectiles(float deltaTimeSeconds)
    {
        m_state.assaultHitTargetId = 0;
        m_state.assaultHitRegionName.clear();
        auto advance = [&](AssaultProjectileState& projectile)
        {
            float flightTime = deltaTimeSeconds;
            const float speed = std::sqrt(projectile.velocity.x * projectile.velocity.x +
                projectile.velocity.y * projectile.velocity.y + projectile.velocity.z * projectile.velocity.z);
            const float remainingDistance = std::max(0.0f,
                projectile.maximumDistanceMeters - projectile.distanceTraveledMeters);
            const bool reachesMaximumDistance = projectile.expireAtMaximumDistance &&
                speed * flightTime >= remainingDistance;
            if (projectile.expireAtMaximumDistance && speed > 0.0f)
                flightTime = std::min(flightTime, remainingDistance / speed);
            if (projectile.lifetimeSeconds > 0.0f)
                flightTime = std::min(flightTime, std::max(0.0f,
                    projectile.lifetimeSeconds - projectile.ageSeconds));
            if (flightTime <= 0.0f) return true;
            Vec3 end = {
                projectile.position.x + projectile.velocity.x * flightTime,
                projectile.position.y + projectile.velocity.y * flightTime,
                projectile.position.z + projectile.velocity.z * flightTime};
            std::uint32_t hitBodyId = JPH::BodyID::cInvalidBodyID;
            Vec3 normal;
            bool staticSurface = false;
            const Vec3 fullEnd = end;
            std::vector<std::uint32_t> ignoredEnemyBodies;
            ignoredEnemyBodies.reserve(m_impl->turretBodyIds.size());
            for (const auto id : m_impl->turretBodyIds)
                if (!id.IsInvalid())
                    ignoredEnemyBodies.push_back(id.GetIndexAndSequenceNumber());
            const bool worldHit = m_impl->controller.CastAssaultSegment(
                projectile.position, end, hitBodyId, normal, staticSurface,
                ignoredEnemyBodies);
            const float worldFraction = worldHit && speed > 0 ? std::sqrt(
                (end.x-projectile.position.x)*(end.x-projectile.position.x) +
                (end.y-projectile.position.y)*(end.y-projectile.position.y) +
                (end.z-projectile.position.z)*(end.z-projectile.position.z)) / (speed * flightTime) : 1.0f;
            float closest = worldFraction;
            EnemyProjectileState* intercepted = nullptr;
            FixedTurretState* hitTank = nullptr;
            TankHitRegionTrace hitRegion;
            for (auto& enemy : m_state.fixedTurrets)
            {
                if (!enemy.target.active) continue;
                const auto hit = TraceTankHitRegions(projectile.position,
                    fullEnd, {enemy.position, enemy.rotation},
                    enemy.unitType.collisionProfile.hitRegions);
                if (hit && hit->fraction < closest)
                {
                    closest = hit->fraction;
                    hitTank = &enemy;
                    hitRegion = *hit;
                }
            }
            for (auto& enemy : m_state.enemyProjectiles)
            {
                if (!enemy.target.active || enemy.target.kind != CombatTargetKind::EnemyProjectile) continue;
                const float t = EnemyInterceptionFraction(projectile.position, fullEnd, enemy, flightTime);
                if (t < closest)
                {
                    closest = t;
                    intercepted = &enemy;
                    hitTank = nullptr;
                }
            }
            if (intercepted)
            {
                AssaultWeapon({projectile.damage, 8}).ApplyHit(intercepted->target);
                m_state.assaultHitTargetId = intercepted->target.id;
                return true;
            }
            if (hitTank)
            {
                const auto result = AssaultWeapon({projectile.damage, 8.0f})
                    .ApplyHit(hitTank->target);
                if (result.hit)
                {
                    m_state.assaultHitTargetId = hitTank->target.id;
                    m_state.assaultHitRegionName = hitTank->unitType
                        .collisionProfile.hitRegions[hitRegion.regionIndex].name;
                    m_state.assaultHitPosition = hitRegion.position;
                    m_state.assaultHitNormal = hitRegion.normal;
                }
                if (result.destroyed)
                {
                    const size_t tankIndex = static_cast<size_t>(
                        hitTank - m_state.fixedTurrets.data());
                    m_impl->DestroyCombatBody(m_impl->turretBodyIds[tankIndex]);
                }
                return true;
            }
            if (worldHit)
            {
                const bool destructible = std::any_of(m_impl->destructibleBodyIds.begin(),
                    m_impl->destructibleBodyIds.end(), [hitBodyId](const auto id)
                    { return !id.IsInvalid() && id.GetIndexAndSequenceNumber() == hitBodyId; });
                // Static Map geometry and the ground retain surface marks. Dynamic
                // destructible targets deliberately remain free of persistent marks.
                const bool turret = std::any_of(m_impl->turretBodyIds.begin(),
                    m_impl->turretBodyIds.end(), [hitBodyId](const auto id)
                    { return !id.IsInvalid() && id.GetIndexAndSequenceNumber() == hitBodyId; });
                if (staticSurface && !destructible && !turret)
                    m_state.assaultImpactMarks.Add(end, normal);
                ApplyAssaultImpact(hitBodyId, projectile.damage);
                return true;
            }
            projectile.position = end;
            projectile.ageSeconds += flightTime;
            projectile.distanceTraveledMeters += speed * flightTime;
            return reachesMaximumDistance || (projectile.lifetimeSeconds > 0.0f &&
                projectile.ageSeconds >= projectile.lifetimeSeconds);
        };
        auto& projectiles = m_state.assaultProjectiles;
        size_t survivors = 0;
        for (size_t i = 0; i < projectiles.size(); ++i)
        {
            auto projectile = projectiles[i];
            if (!advance(projectile)) projectiles[survivors++] = projectile;
        }
        projectiles.resize(survivors);
    }

    void TrackedVehicleTest::UpdateEnemyAttacks(float dt)
    {
        const auto player = m_impl->controller.State().body.position;
        for (size_t i = 0; i < m_state.fixedTurrets.size(); ++i)
        {
            auto& turret = m_state.fixedTurrets[i];
            const auto& q = turret.rotation;
            for (auto& mount : turret.mounts)
            {
                const auto local = JPH::Quat(q.x, q.y, q.z, q.w) * JPH::Vec3(
                    mount.localPosition.x, mount.localPosition.y, mount.localPosition.z);
                const Vec3 origin = {turret.position.x + local.GetX(), turret.position.y + local.GetY(), turret.position.z + local.GetZ()};
                mount.worldPosition = origin;
                bool visible = false;
                if (turret.target.active && mount.attackType.detectionMode == EnemyDetectionMode::Optical)
                {
                    const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z),
                        JPH::Vec3(player.x-origin.x, player.y-origin.y, player.z-origin.z));
                    JPH::RayCastResult hit;
                    visible = !m_impl->world.GetPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, hit, {}, {},
                        JPH::IgnoreSingleBodyFilter(m_impl->turretBodyIds[i])) ||
                        m_impl->controller.IsTankBody(hit.mBodyID.GetIndexAndSequenceNumber());
                }
                const bool active = turret.target.active && m_impl->playerCombat.Snapshot().phase == PlayerCombatPhase::Alive;
                if (!UpdateEnemyAim(mount.aim, mount.attackType, origin, player, dt, visible, active)) continue;
                if (m_state.enemyProjectiles.size() >= kMaximumEnemyProjectiles)
                { --mount.aim.shotsFired; continue; }
                const float horizontal = std::hypot(player.x-origin.x, player.z-origin.z);
                const float pitch = std::atan2(player.y-origin.y, horizontal);
                const float speed = mount.attackType.projectileSpeedMetersPerSecond;
                const Vec3 velocity = {std::sin(mount.aim.yawRadians)*std::cos(pitch)*speed,
                    std::sin(pitch)*speed, std::cos(mount.aim.yawRadians)*std::cos(pitch)*speed};
                const auto kind = mount.attackType.projectileKind == EnemyProjectileKind::Special
                    ? CombatTargetKind::EnemySpecialProjectile : CombatTargetKind::EnemyProjectile;
                m_state.enemyProjectiles.push_back({{m_impl->nextCombatTargetId++, kind, 1, true},
                    turret.target.id, origin, velocity, mount.attackType.reachMeters, mount.attackType.projectileRadiusMeters, 100,
                    mount.attackType.projectileShape, mount.attackType.projectileBoxSizeMeters, EnemyProjectileRotation(velocity)});
            }
        }
    }

    void TrackedVehicleTest::AdvanceEnemyProjectiles(float dt)
    {
        for (auto& bullet : m_state.enemyProjectiles)
        {
            if (!bullet.target.active) continue;
            std::optional<JPH::SphereShape> sphere;
            std::optional<JPH::BoxShape> box;
            if (bullet.shape == EnemyProjectileShape::Box)
                box.emplace(JPH::Vec3(bullet.boxSizeMeters.x, bullet.boxSizeMeters.y, bullet.boxSizeMeters.z)*0.5f, 0.0f);
            else sphere.emplace(bullet.radius);
            const JPH::Shape* shape = box ? static_cast<const JPH::Shape*>(&*box) : static_cast<const JPH::Shape*>(&*sphere);
            const float speed = std::sqrt(bullet.velocity.x*bullet.velocity.x + bullet.velocity.y*bullet.velocity.y + bullet.velocity.z*bullet.velocity.z);
            const float travelTime = speed > 0 ? std::min(dt, bullet.remainingDistance / speed) : 0;
            JPH::BodyID owner;
            for (size_t i = 0; i < m_state.fixedTurrets.size(); ++i)
                if (m_state.fixedTurrets[i].target.id == bullet.ownerId) owner = m_impl->turretBodyIds[i];
            const auto& p = bullet.position;
            const auto& q = bullet.rotation;
            const JPH::RShapeCast cast(shape, JPH::Vec3::sReplicate(1),
                JPH::RMat44::sRotationTranslation(JPH::Quat(q.x, q.y, q.z, q.w), JPH::RVec3(p.x, p.y, p.z)),
                JPH::Vec3(bullet.velocity.x*travelTime, bullet.velocity.y*travelTime, bullet.velocity.z*travelTime));
            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> hit;
            m_impl->world.GetPhysicsSystem().GetNarrowPhaseQuery().CastShape(cast, {}, JPH::RVec3::sZero(), hit,
                {}, {}, JPH::IgnoreSingleBodyFilter(owner));
            if (hit.HadHit())
            {
                if (m_state.invulnerabilitySecondsRemaining <= 0 && m_impl->controller.IsTankBody(hit.mHit.mBodyID2.GetIndexAndSequenceNumber()))
                    m_impl->playerCombat.ApplyDamage(bullet.damage, m_impl->controller.State().body.position);
                const auto hitBody = hit.mHit.mBodyID2.GetIndexAndSequenceNumber();
                if (std::any_of(m_impl->destructibleBodyIds.begin(), m_impl->destructibleBodyIds.end(),
                    [&](const auto& id) {return !id.IsInvalid() && id.GetIndexAndSequenceNumber() == hitBody;}))
                    ApplyAssaultImpact(hitBody, bullet.damage);
                bullet.target.active = false;
                continue;
            }
            bullet.position = {p.x+bullet.velocity.x*travelTime, p.y+bullet.velocity.y*travelTime, p.z+bullet.velocity.z*travelTime};
            bullet.remainingDistance -= speed*travelTime;
            if (bullet.remainingDistance <= 1.0e-5f || speed <= 0) bullet.target.active = false;
        }
        std::erase_if(m_state.enemyProjectiles, [](const auto& bullet) { return !bullet.target.active; });
    }

    void TrackedVehicleTest::ApplyAssaultImpact(std::uint32_t hitBodyId, float damage)
    {
        for (size_t i = 0; i < m_impl->destructibleBodyIds.size(); ++i)
        {
            auto& id = m_impl->destructibleBodyIds[i];
            if (id.IsInvalid() || id.GetIndexAndSequenceNumber() != hitBodyId) continue;
            auto& target = m_state.destructibleBoxes[i].target;
            const auto result = AssaultWeapon({damage, 8.0f}).ApplyHit(target);
            if (result.hit) m_state.assaultHitTargetId = target.id;
            if (result.destroyed)
            {
                m_impl->DestroyCombatBody(id);
            }
            break;
        }
        for (size_t i = 0; i < m_impl->turretBodyIds.size(); ++i)
        {
            auto& id = m_impl->turretBodyIds[i];
            if (id.IsInvalid() || id.GetIndexAndSequenceNumber() != hitBodyId) continue;
            auto& target = m_state.fixedTurrets[i].target;
            const auto result = AssaultWeapon({damage, 8.0f}).ApplyHit(target);
            if (result.hit) m_state.assaultHitTargetId = target.id;
            if (result.destroyed)
            {
                m_impl->DestroyCombatBody(id);
            }
            break;
        }
    }

    void TrackedVehicleTest::SpawnMortarShell()
    {
        const auto& shot = m_impl->controller.State().pendingMortarShot;
        m_impl->resolvedMortarShots = shot.sequence;
        const int maximumCount = (std::max)(
            0, m_impl->controller.Settings().mortarMaximumProjectileCount);
        if (m_state.mortarProjectiles.size() >= static_cast<size_t>(maximumCount))
            return;
        m_state.mortarProjectiles.push_back({
            shot.origin,
            shot.velocity,
            shot.blastRadiusMeters,
            (std::max)(0.0f, m_impl->controller.Settings().mortarExplosionDamage),
            0.0f,
            shot.gravityScale});
    }

    void TrackedVehicleTest::AdvanceMortarProjectiles(float deltaTimeSeconds)
    {
        m_state.mortarHitTargetId = 0;
        for (auto& blast : m_state.mortarBlasts)
            blast.ageSeconds += deltaTimeSeconds;
        std::erase_if(m_state.mortarBlasts, [](const MortarBlastState& blast)
            { return blast.ageSeconds >= blast.durationSeconds; });

        // Semi-implicit Euler integration; the shell is not a Jolt body.
        constexpr float kGravityMetersPerSecondSquared = 9.81f;
        constexpr float kMaximumFlightSeconds = 30.0f;
        auto& projectiles = m_state.mortarProjectiles;
        size_t survivors = 0;
        for (size_t i = 0; i < projectiles.size(); ++i)
        {
            auto projectile = projectiles[i];
            projectile.velocity.y -= kGravityMetersPerSecondSquared *
                projectile.gravityScale * deltaTimeSeconds;
            Vec3 end = {
                projectile.position.x + projectile.velocity.x * deltaTimeSeconds,
                projectile.position.y + projectile.velocity.y * deltaTimeSeconds,
                projectile.position.z + projectile.velocity.z * deltaTimeSeconds};
            std::uint32_t hitBodyId = JPH::BodyID::cInvalidBodyID;
            Vec3 normal;
            bool staticSurface = false;
            if (m_impl->controller.CastAssaultSegment(
                projectile.position, end, hitBodyId, normal, staticSurface))
            {
                ApplyMortarBlast(end, projectile.blastRadiusMeters, projectile.damage);
                if (staticSurface) m_state.assaultImpactMarks.Add(end, normal);
            }
            else
            {
                projectile.position = end;
                projectile.ageSeconds += deltaTimeSeconds;
                if (projectile.ageSeconds < kMaximumFlightSeconds && end.y > -50.0f)
                    projectiles[survivors++] = projectile;
            }
        }
        projectiles.resize(survivors);
    }

    void TrackedVehicleTest::ApplyMortarBlast(const Vec3& center, float radius, float damage)
    {
        constexpr float kBlastEffectSeconds = 0.5f;
        if (radius > 0.0f)
            m_state.mortarBlasts.push_back({center, radius, 0.0f, kBlastEffectSeconds});
        const float safeRadius = (std::max)(radius, 0.01f);
        for (size_t i = 0; i < m_impl->destructibleBodyIds.size(); ++i)
        {
            auto& id = m_impl->destructibleBodyIds[i];
            if (id.IsInvalid()) continue;
            const auto& box = m_state.destructibleBoxes[i];
            // Distance from the blast center to the closest point on the box AABB.
            const float dx = center.x - std::clamp(center.x,
                box.position.x - 0.5f * box.size.x, box.position.x + 0.5f * box.size.x);
            const float dy = center.y - std::clamp(center.y,
                box.position.y - 0.5f * box.size.y, box.position.y + 0.5f * box.size.y);
            const float dz = center.z - std::clamp(center.z,
                box.position.z - 0.5f * box.size.z, box.position.z + 0.5f * box.size.z);
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance > radius) continue;
            const float falloff = (std::max)(1.0f - distance / safeRadius, 0.1f);
            auto& target = m_state.destructibleBoxes[i].target;
            const auto result = AssaultWeapon({damage * falloff, 8.0f}).ApplyHit(target);
            if (result.hit) m_state.mortarHitTargetId = target.id;
            if (result.destroyed)
            {
                m_impl->DestroyCombatBody(id);
            }
        }
        for (size_t i = 0; i < m_impl->turretBodyIds.size(); ++i)
        {
            auto& id = m_impl->turretBodyIds[i];
            if (id.IsInvalid()) continue;
            auto& turret = m_state.fixedTurrets[i];
            // Measure blast distance to the rotated collision box, in its local space.
            const auto& q = turret.rotation;
            const auto local = JPH::Quat(q.x, q.y, q.z, q.w).Conjugated() * JPH::Vec3(
                center.x-turret.position.x, center.y-turret.position.y, center.z-turret.position.z);
            const float dx = (std::max)(std::abs(local.GetX())-0.5f*turret.size.x, 0.0f);
            const float dy = (std::max)(std::abs(local.GetY())-0.5f*turret.size.y, 0.0f);
            const float dz = (std::max)(std::abs(local.GetZ())-0.5f*turret.size.z, 0.0f);
            const float distance = std::sqrt(dx*dx+dy*dy+dz*dz);
            if (distance > radius) continue;
            const float falloff = (std::max)(1.0f-distance/safeRadius, 0.1f);
            const auto result = AssaultWeapon({damage*falloff, 8.0f}).ApplyHit(turret.target);
            if (result.hit) m_state.mortarHitTargetId = turret.target.id;
            if (result.destroyed)
            {
                m_impl->DestroyCombatBody(id);
            }
        }
        for (auto& bullet : m_state.enemyProjectiles)
        {
            if (!bullet.target.active || bullet.target.kind != CombatTargetKind::EnemyProjectile) continue;
            const float dx=bullet.position.x-center.x, dy=bullet.position.y-center.y, dz=bullet.position.z-center.z;
            const float combinedRadius = radius+bullet.radius;
            if (radius <= 0 || dx*dx+dy*dy+dz*dz > combinedRadius*combinedRadius) continue;
            const auto result = AssaultWeapon({damage, 8.0f}).ApplyHit(bullet.target);
            if (result.hit) m_state.mortarHitTargetId = bullet.target.id;
        }
    }

    bool TrackedVehicleTest::ApplyRecoilImpulse(float impulseNewtonSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        return m_impl->controller.ApplyRecoilImpulse(impulseNewtonSeconds);
    }

    TrackedVehicleTestState TrackedVehicleTest::Step(float deltaTimeSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize();
        }

        if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f)
        {
            return m_state;
        }

        const auto phase = m_impl->playerCombat.Snapshot().phase;
        if (phase == PlayerCombatPhase::GameOver) return m_state;
        m_impl->enemyObservationTimeSeconds += deltaTimeSeconds;
        if (phase == PlayerCombatPhase::Lost)
        {
            m_state.respawnSecondsRemaining = std::max(0.0f, m_state.respawnSecondsRemaining - deltaTimeSeconds);
            if (m_state.respawnSecondsRemaining > 0.00001f) return m_state;
            m_state.respawnSecondsRemaining = 0;
            const auto position = m_impl->playerCombat.Snapshot().lostPosition;
            const auto settings = m_impl->controller.Settings();
            m_impl->controller.Initialize(m_impl->world, settings, {position, m_impl->lostYaw});
            m_impl->resolvedAssaultRounds = 0;
            m_impl->resolvedMortarShots = 0;
            m_state.mortarShotsFired = 0;
            m_state.assaultWeapon = {};
            m_impl->playerCombat.Respawn();
            m_state.playerCombat = m_impl->playerCombat.Snapshot();
            m_state.bodyPosition = position;
            m_state.bodyRotation = {0, std::sin(m_impl->lostYaw*0.5f), 0, std::cos(m_impl->lostYaw*0.5f)};
            m_impl->requireNeutralInput = true;
            m_state.linearVelocity = {};
            m_state.angularVelocity = {};
            m_state.invulnerabilitySecondsRemaining = 2;
            ++m_state.respawnCount;
            return m_state;
        }
        m_state.invulnerabilitySecondsRemaining = std::max(0.0f, m_state.invulnerabilitySecondsRemaining - deltaTimeSeconds);
        AdvanceAssaultProjectiles(deltaTimeSeconds);
        AdvanceMortarProjectiles(deltaTimeSeconds);
        AdvanceEnemyProjectiles(deltaTimeSeconds);
        if (m_impl->playerCombat.Snapshot().phase != PlayerCombatPhase::Alive)
        {
            m_state.playerCombat = m_impl->playerCombat.Snapshot();
            UpdateEnemyGroups(deltaTimeSeconds);
            m_state.respawnSecondsRemaining = m_state.playerCombat.phase == PlayerCombatPhase::Lost ? 1.0f : 0;
            const auto q = m_impl->controller.State().body.rotation;
            m_impl->lostYaw = std::atan2(2*(q.x*q.z+q.y*q.w), 1-2*(q.x*q.x+q.y*q.y));
            m_state.assaultProjectiles.clear();
            m_state.enemyProjectiles.clear();
            m_state.mortarProjectiles.clear();
            m_state.mortarBlasts.clear();
            m_impl->controller.SetInput({});
            return m_state;
        }
        m_impl->controller.SetAssaultCapacityAvailable(
            m_state.assaultProjectiles.size() < static_cast<size_t>(m_impl->projectileSettings.maximumCount));
        auto& bodies = m_impl->world.GetBodyInterface();
        for (size_t i = 0; i < m_state.fixedTurrets.size(); ++i)
        {
            auto& enemy = m_state.fixedTurrets[i];
            if (!enemy.target.active || enemy.supportReleased || enemy.supportPlacementId.empty()) continue;
            const auto support = std::find_if(m_state.destructibleBoxes.begin(), m_state.destructibleBoxes.end(),
                [&](const auto& object) {return object.placementId == enemy.supportPlacementId;});
            if (support == m_state.destructibleBoxes.end() || support->target.active) continue;
            const auto id = m_impl->turretBodyIds[i];
            if (id.IsInvalid()) continue;
            bodies.SetMotionType(id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            bodies.SetObjectLayer(id, Layers::Moving);
            bodies.ActivateBody(id);
            enemy.supportReleased = true;
            enemy.simpleMoving = false;
        }
        for (size_t i = 0; i < m_state.gates.size(); ++i)
        {
            auto& gate = m_state.gates[i];
            gate.Advance({m_state.bodyPosition.x, m_state.bodyPosition.y, m_state.bodyPosition.z}, deltaTimeSeconds);
            const auto offset = gate.Offset();
            bodies.MoveKinematic(m_impl->gateBodyIds[i], JPH::RVec3(offset[0], offset[1], offset[2]),
                JPH::Quat::sIdentity(), deltaTimeSeconds);
        }
        AdvanceEnemyNavigation(deltaTimeSeconds);
        m_impl->controller.PreStep();
        m_impl->world.Step(deltaTimeSeconds);
        m_impl->controller.PostStep(deltaTimeSeconds);
        for (auto& driver : m_impl->navigationDrivers)
            if (driver.physical)
            {
                driver.physical->PostStep(deltaTimeSeconds);
                auto& enemy = m_state.fixedTurrets[driver.enemyIndex];
                const auto& state = driver.physical->State();
                enemy.position = state.body.position;
                enemy.rotation = state.body.rotation;
                enemy.navigationVelocity = state.linearVelocity;
                enemy.navigationYawRadiansPerSecond = state.angularVelocity.y;
                enemy.runningGear = EnemyRunningGearState{state.wheels, state.wheelCount,
                    {state.leftTrackAngularVelocityRadians, state.rightTrackAngularVelocityRadians}};
            }

        for (size_t i = 0; i < m_state.fixedTurrets.size(); ++i)
        {
            auto& enemy = m_state.fixedTurrets[i];
            const auto id = m_impl->turretBodyIds[i];
            if (!enemy.target.active || !enemy.supportReleased || id.IsInvalid()) continue;
            const auto p = bodies.GetPosition(id);
            const auto q = bodies.GetRotation(id);
            enemy.position = {static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
            enemy.rotation = {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
        }
        m_state.stepIndex = m_impl->controller.State().stepIndex;
        m_state.timeSeconds = m_impl->controller.State().timeSeconds;
        m_state.bodyPosition = m_impl->controller.State().body.position;
        m_state.bodyRotation = m_impl->controller.State().body.rotation;
        m_state.linearVelocity = m_impl->controller.State().linearVelocity;
        m_state.angularVelocity = m_impl->controller.State().angularVelocity;
        m_state.speedMetersPerSecond =
            m_impl->controller.State().speedMetersPerSecond;
        m_state.maximumSpeedMetersPerSecond =
            m_impl->controller.State().maximumSpeedMetersPerSecond;
        m_state.zeroToTenTimeSeconds =
            m_impl->controller.State().zeroToTenTimeSeconds;
        m_state.engineRpm = m_impl->controller.State().engineRpm;
        m_state.transmissionGear = m_impl->controller.State().transmissionGear;
        m_state.clutchFriction = m_impl->controller.State().clutchFriction;
        m_state.transmissionSwitchingGear =
            m_impl->controller.State().transmissionSwitchingGear;
        m_state.leftTrackAngularVelocityRadians =
            m_impl->controller.State().leftTrackAngularVelocityRadians;
        m_state.rightTrackAngularVelocityRadians =
            m_impl->controller.State().rightTrackAngularVelocityRadians;
        m_state.leftTrackDriveTorqueNm =
            m_impl->controller.State().leftTrackDriveTorqueNm;
        m_state.rightTrackDriveTorqueNm =
            m_impl->controller.State().rightTrackDriveTorqueNm;
        m_state.yawSpeedDegrees = m_impl->controller.State().yawSpeedDegrees;
        m_state.yawSpeedLimited = m_impl->controller.State().yawSpeedLimited;
        m_state.wheels = m_impl->controller.State().wheels;
        m_state.wheelCount = m_impl->controller.State().wheelCount;
        m_state.sleeping = m_impl->controller.State().sleeping;
        m_state.motionObservation =
            m_impl->controller.State().motionObservation;
        m_state.mobility = m_impl->controller.State().mobility;
        m_state.rollingPhase = m_impl->controller.State().rollingPhase;
        m_state.rollingTelemetry = m_impl->controller.State().rollingTelemetry;
        m_state.lastRollingDecision =
            m_impl->controller.State().lastRollingDecision;
        m_state.rollingDecisionCount =
            m_impl->controller.State().rollingDecisionCount;
        m_state.rollingDecisionCommandSign =
            m_impl->controller.State().rollingDecisionCommandSign;
        m_state.rollingDecisionInputSign =
            m_impl->controller.State().rollingDecisionInputSign;
        m_state.lastRollingTraceEvent =
            m_impl->controller.State().lastRollingTraceEvent;
        m_state.rollingTraceSequence =
            m_impl->controller.State().rollingTraceSequence;
        m_state.rollingTraceRequestSign =
            m_impl->controller.State().rollingTraceRequestSign;
        m_state.rollingTraceCommandSign =
            m_impl->controller.State().rollingTraceCommandSign;
        m_state.rollingTraceInputSign =
            m_impl->controller.State().rollingTraceInputSign;
        m_state.specialMove = m_impl->controller.State().specialMove;
        m_state.mortarAim = m_impl->controller.State().mortarAim;
        m_state.assaultWeapon = m_impl->controller.State().assaultWeapon;
        if (m_state.assaultWeapon.roundsFired != m_impl->resolvedAssaultRounds)
        {
            SpawnAssaultRound();
        }
        m_state.mortarShotsFired = m_impl->controller.State().mortarShotsFired;
        if (m_impl->controller.State().pendingMortarShot.sequence != 0 &&
            m_impl->controller.State().pendingMortarShot.sequence != m_impl->resolvedMortarShots)
        {
            SpawnMortarShell();
        }
        m_state.trackInputSwapped =
            m_impl->controller.State().trackInputSwapped;
        m_state.rollChainAvailable =
            m_impl->controller.State().rollChainAvailable;

        UpdateEnemyAttacks(deltaTimeSeconds);
        UpdateEnemyGroups(deltaTimeSeconds);
        m_state.playerCombat = m_impl->playerCombat.Snapshot();
        return m_state;
    }
}
