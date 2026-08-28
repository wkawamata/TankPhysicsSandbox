#pragma once

#include <cstdint>

namespace Tank::Physics
{
    struct SpecialActionInput
    {
        float leftLeverHorizontal = 0.0f;
        float rightLeverHorizontal = 0.0f;
    };

    enum class SpecialAction
    {
        None,
        MortarRequested,
        RollLeftRequested,
        RollRightRequested
    };

    struct SpecialActionRecognizerSettings
    {
        float startThreshold = 0.70f;
        float releaseThreshold = 0.20f;
    };

    class SpecialActionRecognizer
    {
    public:
        explicit SpecialActionRecognizer(
            const SpecialActionRecognizerSettings& settings = {});

        void Reset();
        SpecialAction Update(const SpecialActionInput& input);
        bool IsArmed() const { return m_armed; }

    private:
        bool IsNeutral(const SpecialActionInput& input) const;
        bool IsOutward(const SpecialActionInput& input) const;
        bool IsSameDirection(const SpecialActionInput& input) const;

        SpecialActionRecognizerSettings m_settings = {};
        bool m_armed = true;
    };
}
