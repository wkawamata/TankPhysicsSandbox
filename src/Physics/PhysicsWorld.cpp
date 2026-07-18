#include "PhysicsWorld.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
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
    }

    struct PhysicsWorld::Impl
    {
        BPLayerInterfaceImpl broadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilterImpl objectLayerPairFilter;
        BodyActivationListenerImpl bodyActivationListener;
        JPH::TempAllocatorImpl tempAllocator {10 * 1024 * 1024};
        JPH::JobSystemThreadPool jobSystem;
        JPH::PhysicsSystem physicsSystem;

        Impl()
            : jobSystem(
                JPH::cMaxPhysicsJobs,
                JPH::cMaxPhysicsBarriers,
                static_cast<int>(std::max(1u, std::thread::hardware_concurrency()) - 1))
        {
        }
    };

    PhysicsWorld::PhysicsWorld() = default;
    PhysicsWorld::~PhysicsWorld() = default;

    void PhysicsWorld::Initialize()
    {
        EnsureJoltRuntime();

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
    }

    JPH::BodyInterface& PhysicsWorld::GetBodyInterface()
    {
        return m_impl->physicsSystem.GetBodyInterface();
    }

    JPH::PhysicsSystem& PhysicsWorld::GetPhysicsSystem()
    {
        return m_impl->physicsSystem;
    }

    void PhysicsWorld::Step(float deltaTimeSeconds)
    {
        m_impl->physicsSystem.Update(deltaTimeSeconds, 1, &m_impl->tempAllocator, &m_impl->jobSystem);
    }
}
