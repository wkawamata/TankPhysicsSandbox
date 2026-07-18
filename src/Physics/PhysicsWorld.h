#pragma once

#include <memory>

namespace JPH
{
    class BodyInterface;
    class PhysicsSystem;
}

namespace Tank::Physics
{
    class PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        void Initialize();
        JPH::BodyInterface& GetBodyInterface();
        JPH::PhysicsSystem& GetPhysicsSystem();
        void Step(float deltaTimeSeconds);

    private:
        struct Impl;

        std::unique_ptr<Impl> m_impl;
    };
}
