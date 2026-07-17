#include "BoxDropTest.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>

JPH_SUPPRESS_WARNINGS

namespace Tank::Physics
{
    namespace
    {
        void TraceImpl(const char* format, ...)
        {
            va_list list;
            va_start(list, format);
            char buffer[1024];
            std::vsnprintf(buffer, sizeof(buffer), format, list);
            va_end(list);
        }

#ifdef JPH_ENABLE_ASSERTS
        bool AssertFailedImpl(const char* expression, const char* message, const char* file, JPH::uint line)
        {
            (void)expression;
            (void)message;
            (void)file;
            (void)line;
            return true;
        }
#endif

        void EnsureJoltRuntime()
        {
            static std::once_flag initialized;
            std::call_once(initialized, []()
            {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = TraceImpl;
                JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

                if (JPH::Factory::sInstance == nullptr)
                {
                    JPH::Factory::sInstance = new JPH::Factory();
                    JPH::RegisterTypes();
                }
            });
        }

        namespace Layers
        {
            constexpr JPH::ObjectLayer NonMoving = 0;
            constexpr JPH::ObjectLayer Moving = 1;
            constexpr JPH::ObjectLayer Count = 2;
        }

        class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override
            {
                switch (object1)
                {
                case Layers::NonMoving:
                    return object2 == Layers::Moving;
                case Layers::Moving:
                    return true;
                default:
                    return false;
                }
            }
        };

        namespace BroadPhaseLayers
        {
            const JPH::BroadPhaseLayer NonMoving(0);
            const JPH::BroadPhaseLayer Moving(1);
            constexpr JPH::uint Count = 2;
        }

        class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
        public:
            BPLayerInterfaceImpl()
            {
                m_objectToBroadPhase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
                m_objectToBroadPhase[Layers::Moving] = BroadPhaseLayers::Moving;
            }

            JPH::uint GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::Count;
            }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                JPH_ASSERT(layer < Layers::Count);
                return m_objectToBroadPhase[layer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
            {
                if (layer == BroadPhaseLayers::NonMoving)
                {
                    return "NonMoving";
                }

                if (layer == BroadPhaseLayers::Moving)
                {
                    return "Moving";
                }

                return "Invalid";
            }
#endif

        private:
            JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::Count];
        };

        class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override
            {
                switch (layer1)
                {
                case Layers::NonMoving:
                    return layer2 == BroadPhaseLayers::Moving;
                case Layers::Moving:
                    return true;
                default:
                    return false;
                }
            }
        };

        class BodyActivationListenerImpl final : public JPH::BodyActivationListener
        {
        public:
            void OnBodyActivated(const JPH::BodyID&, JPH::uint64) override {}
            void OnBodyDeactivated(const JPH::BodyID&, JPH::uint64) override {}
        };

        JPH::QuatArg ToJoltQuat(const JPH::Quat& rotation)
        {
            return rotation;
        }
    }

    struct BoxDropTest::Impl
    {
        BPLayerInterfaceImpl broadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilterImpl objectLayerPairFilter;
        BodyActivationListenerImpl bodyActivationListener;
        JPH::TempAllocatorImpl tempAllocator {10 * 1024 * 1024};
        JPH::JobSystemThreadPool jobSystem;
        JPH::PhysicsSystem physicsSystem;
        JPH::BodyID floorBodyId;
        JPH::BodyID boxBodyId;
        bool hasFloorBody = false;
        bool hasBoxBody = false;

        Impl()
            : jobSystem(
                JPH::cMaxPhysicsJobs,
                JPH::cMaxPhysicsBarriers,
                static_cast<int>(std::max(1u, std::thread::hardware_concurrency()) - 1))
        {
        }

        ~Impl()
        {
            JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
            if (hasBoxBody)
            {
                bodyInterface.RemoveBody(boxBodyId);
                bodyInterface.DestroyBody(boxBodyId);
            }

            if (hasFloorBody)
            {
                bodyInterface.RemoveBody(floorBodyId);
                bodyInterface.DestroyBody(floorBodyId);
            }
        }
    };

    BoxDropTest::BoxDropTest() = default;
    BoxDropTest::~BoxDropTest() = default;

    void BoxDropTest::Initialize(const BoxDropSettings& settings)
    {
        EnsureJoltRuntime();

        m_settings = settings;
        m_state = {};
        m_state.boxPosition = {0.0f, m_settings.initialHeight, 0.0f};
        m_state.boxRotation = {0.0f, 0.0f, 0.0f, 1.0f};

        m_impl = std::make_unique<Impl>();

        constexpr JPH::uint maxBodies = 1024;
        constexpr JPH::uint numBodyMutexes = 0;
        constexpr JPH::uint maxBodyPairs = 1024;
        constexpr JPH::uint maxContactConstraints = 1024;

        m_impl->physicsSystem.Init(
            maxBodies,
            numBodyMutexes,
            maxBodyPairs,
            maxContactConstraints,
            m_impl->broadPhaseLayerInterface,
            m_impl->objectVsBroadPhaseLayerFilter,
            m_impl->objectLayerPairFilter);
        m_impl->physicsSystem.SetBodyActivationListener(&m_impl->bodyActivationListener);

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();

        JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
        floorShapeSettings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
        JPH::BodyCreationSettings floorSettings(
            floorShapeResult.Get().GetPtr(),
            JPH::RVec3(0.0, static_cast<double>(m_settings.floorTopY) - 1.0, 0.0),
            ToJoltQuat(JPH::Quat::sIdentity()),
            JPH::EMotionType::Static,
            Layers::NonMoving);

        JPH::Body* floorBody = bodyInterface.CreateBody(floorSettings);
        m_impl->floorBodyId = floorBody->GetID();
        m_impl->hasFloorBody = true;
        bodyInterface.AddBody(m_impl->floorBodyId, JPH::EActivation::DontActivate);

        JPH::BodyCreationSettings boxSettings(
            new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f)),
            JPH::RVec3(0.0, static_cast<double>(m_settings.initialHeight), 0.0),
            ToJoltQuat(JPH::Quat::sIdentity()),
            JPH::EMotionType::Dynamic,
            Layers::Moving);

        m_impl->boxBodyId = bodyInterface.CreateAndAddBody(boxSettings, JPH::EActivation::Activate);
        m_impl->hasBoxBody = true;
    }

    BoxDropState BoxDropTest::Step(float deltaTimeSeconds)
    {
        if (m_impl == nullptr)
        {
            Initialize(m_settings);
        }

        m_impl->physicsSystem.Update(deltaTimeSeconds, 1, &m_impl->tempAllocator, &m_impl->jobSystem);

        m_state.stepIndex++;
        m_state.timeSeconds += deltaTimeSeconds;

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        const JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(m_impl->boxBodyId);
        const JPH::Quat rotation = bodyInterface.GetRotation(m_impl->boxBodyId);
        m_state.boxPosition = {
            static_cast<float>(position.GetX()),
            static_cast<float>(position.GetY()),
            static_cast<float>(position.GetZ())};
        m_state.boxRotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()};
        m_state.boxSleeping = !bodyInterface.IsActive(m_impl->boxBodyId);

        return m_state;
    }
}
