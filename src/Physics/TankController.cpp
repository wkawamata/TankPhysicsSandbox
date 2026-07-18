#include "TankController.h"

#include <algorithm>

namespace Tank::Physics
{
    namespace
    {
        float ClampNormalized(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }
    }

    void TankController::Initialize()
    {
        m_input = {};
        m_state = {};
    }

    void TankController::SetInput(const TankInput& input)
    {
        m_input.throttle = ClampNormalized(input.throttle);
        m_input.steering = ClampNormalized(input.steering);
        m_input.leftTrack = ClampNormalized(input.leftTrack);
        m_input.rightTrack = ClampNormalized(input.rightTrack);
        m_input.brake = input.brake;
    }

    void TankController::Step(float deltaTimeSeconds)
    {
        if (deltaTimeSeconds <= 0.0f)
        {
            return;
        }

        m_state.stepIndex++;
        m_state.timeSeconds += deltaTimeSeconds;
    }
}
