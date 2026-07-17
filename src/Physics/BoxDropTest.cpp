#include "BoxDropTest.h"

namespace Tank::Physics
{
    void BoxDropTest::Initialize(const BoxDropSettings& settings)
    {
        m_settings = settings;
        m_state = {};
        m_state.boxPosition = {0.0f, m_settings.initialHeight, 0.0f};
        m_state.boxRotation = {};
    }

    BoxDropState BoxDropTest::Step(float deltaTimeSeconds)
    {
        m_state.stepIndex++;
        m_state.timeSeconds += deltaTimeSeconds;
        return m_state;
    }
}
