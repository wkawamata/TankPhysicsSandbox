#include "SpecialActionRecognizer.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    SpecialActionRecognizer::SpecialActionRecognizer(
        const SpecialActionRecognizerSettings& settings)
        : m_settings(settings)
    {
        m_settings.startThreshold = std::clamp(
            std::isfinite(settings.startThreshold)
                ? settings.startThreshold : 0.70f,
            0.05f,
            1.0f);
        m_settings.releaseThreshold = std::clamp(
            std::isfinite(settings.releaseThreshold)
                ? settings.releaseThreshold : 0.20f,
            0.0f,
            m_settings.startThreshold);
    }

    void SpecialActionRecognizer::Reset()
    {
        m_armed = true;
    }

    SpecialAction SpecialActionRecognizer::Update(
        const SpecialActionInput& input)
    {
        if (!std::isfinite(input.leftLeverHorizontal) ||
            !std::isfinite(input.rightLeverHorizontal))
        {
            m_armed = false;
            return SpecialAction::None;
        }

        if (!m_armed)
        {
            if (IsNeutral(input))
            {
                m_armed = true;
            }
            return SpecialAction::None;
        }

        SpecialAction action = SpecialAction::None;
        if (IsOutward(input))
        {
            action = SpecialAction::MortarRequested;
        }
        else if (IsSameDirection(input))
        {
            action = input.leftLeverHorizontal < 0.0f
                ? SpecialAction::RollLeftRequested
                : SpecialAction::RollRightRequested;
        }

        if (action != SpecialAction::None)
        {
            m_armed = false;
        }
        return action;
    }

    bool SpecialActionRecognizer::IsNeutral(
        const SpecialActionInput& input) const
    {
        return std::abs(input.leftLeverHorizontal) <=
                m_settings.releaseThreshold &&
            std::abs(input.rightLeverHorizontal) <=
                m_settings.releaseThreshold;
    }

    bool SpecialActionRecognizer::IsOutward(
        const SpecialActionInput& input) const
    {
        return input.leftLeverHorizontal <= -m_settings.startThreshold &&
            input.rightLeverHorizontal >= m_settings.startThreshold;
    }

    bool SpecialActionRecognizer::IsSameDirection(
        const SpecialActionInput& input) const
    {
        const bool left = std::abs(input.leftLeverHorizontal) >=
            m_settings.startThreshold;
        const bool right = std::abs(input.rightLeverHorizontal) >=
            m_settings.startThreshold;
        return left && right &&
            ((input.leftLeverHorizontal < 0.0f &&
                input.rightLeverHorizontal < 0.0f) ||
             (input.leftLeverHorizontal > 0.0f &&
                input.rightLeverHorizontal > 0.0f));
    }
}
