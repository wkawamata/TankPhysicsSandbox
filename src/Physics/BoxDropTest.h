#pragma once

#include "PhysicsTypes.h"

#include <memory>

namespace Tank::Physics
{
    struct BoxDropSettings
    {
        float initialHeight = 5.0f;
        float floorTopY = 0.0f;
    };

    struct BoxDropState
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        Vec3 boxPosition = {};
        Quat boxRotation = {};
        bool boxSleeping = false;
    };

    class BoxDropTest
    {
    public:
        BoxDropTest();
        ~BoxDropTest();

        BoxDropTest(const BoxDropTest&) = delete;
        BoxDropTest& operator=(const BoxDropTest&) = delete;

        void Initialize(const BoxDropSettings& settings = {});
        BoxDropState Step(float deltaTimeSeconds);
        const BoxDropState& State() const { return m_state; }

    private:
        struct Impl;

        BoxDropSettings m_settings = {};
        BoxDropState m_state = {};
        std::unique_ptr<Impl> m_impl;
    };
}
