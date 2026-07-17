#pragma once

#include "PhysicsTypes.h"

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
        void Initialize(const BoxDropSettings& settings = {});
        BoxDropState Step(float deltaTimeSeconds);
        const BoxDropState& State() const { return m_state; }

    private:
        BoxDropSettings m_settings = {};
        BoxDropState m_state = {};
    };
}
