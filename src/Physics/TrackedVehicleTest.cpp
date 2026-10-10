#include "TrackedVehicleTest.h"
#include "PhysicsWorld.h"
#include "TankController.h"
#include "StaticMeshShape.h"

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
        std::vector<JPH::BodyID> destructibleBodyIds;
        std::vector<JPH::BodyID> turretBodyIds;
        std::uint64_t nextCombatTargetId = 1;
        std::uint64_t resolvedAssaultRounds = 0;
        std::uint64_t resolvedMortarShots = 0;
        AssaultProjectileSettings projectileSettings = {};
        std::vector<EnemyAttackType> enemyAttackTypes;
        PlayerCombatState playerCombat;
        float lostYaw = 0;
        bool requireNeutralInput = false;
        bool hasFloorBody = false;

        ~Impl()
        {
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
                JPH::BodyID bodyId;
                std::string shapeError;
                if (!AddStaticMeshBody(m_impl->world, mesh, bodyId, shapeError))
                {
                    if (error != nullptr) *error = shapeError;
                    return false;
                }
                m_impl->obstacleBodyIds.push_back(bodyId);
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
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(JPH::Vec3(size.x, size.y, size.z) * 0.5f),
            JPH::RVec3(position.x, position.y, position.z), JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
            JPH::EMotionType::Static, Layers::NonMoving);
        auto& bodies = m_impl->world.GetBodyInterface();
        JPH::Body* body = bodies.CreateBody(settings);
        if (!body) return false;
        const auto id = body->GetID();
        bodies.AddBody(id, JPH::EActivation::DontActivate);
        m_impl->turretBodyIds.push_back(id);
        m_state.fixedTurrets.push_back({
            {m_impl->nextCombatTargetId++, CombatTargetKind::Enemy, hitPoints, true},
            position, size, rotation, placementId, unitType, {}});
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
            const bool worldHit = m_impl->controller.CastAssaultSegment(projectile.position, end, hitBodyId, normal, staticSurface);
            const float worldFraction = worldHit && speed > 0 ? std::sqrt(
                (end.x-projectile.position.x)*(end.x-projectile.position.x) +
                (end.y-projectile.position.y)*(end.y-projectile.position.y) +
                (end.z-projectile.position.z)*(end.z-projectile.position.z)) / (speed * flightTime) : 1.0f;
            float closest = worldFraction;
            EnemyProjectileState* intercepted = nullptr;
            for (auto& enemy : m_state.enemyProjectiles)
            {
                if (!enemy.target.active || enemy.target.kind != CombatTargetKind::EnemyProjectile) continue;
                const float t = EnemyInterceptionFraction(projectile.position, fullEnd, enemy, flightTime);
                if (t < closest) { closest = t; intercepted = &enemy; }
            }
            if (intercepted)
            {
                AssaultWeapon({projectile.damage, 8}).ApplyHit(intercepted->target);
                m_state.assaultHitTargetId = intercepted->target.id;
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
                auto& bodies = m_impl->world.GetBodyInterface();
                bodies.RemoveBody(id);
                bodies.DestroyBody(id);
                id = JPH::BodyID();
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
                auto& bodies = m_impl->world.GetBodyInterface();
                bodies.RemoveBody(id);
                bodies.DestroyBody(id);
                id = JPH::BodyID();
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
                auto& bodies = m_impl->world.GetBodyInterface();
                bodies.RemoveBody(id);
                bodies.DestroyBody(id);
                id = JPH::BodyID();
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
                auto& bodies = m_impl->world.GetBodyInterface();
                bodies.RemoveBody(id);
                bodies.DestroyBody(id);
                id = JPH::BodyID();
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
        m_impl->controller.PreStep();
        m_impl->world.Step(deltaTimeSeconds);
        m_impl->controller.PostStep(deltaTimeSeconds);

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
        m_state.playerCombat = m_impl->playerCombat.Snapshot();
        return m_state;
    }
}
