#pragma once

#include "SpecialActionInputAdapter.h"
#include "SpecialMoveStateMachine.h"

namespace Tank::Physics
{
    class SpecialMoveInputProcessor
    {
    public:
        const SpecialMoveStateSnapshot& Update(
            SpecialMoveStateMachine& stateMachine,
            const TankInput& input,
            bool mobilityStopped)
        {
            const SpecialAction action =
                SpecialActionInputAdapter::Update(m_recognizer, input);
            SpecialMoveEvent event = SpecialMoveEvent::None;
            switch (action)
            {
            case SpecialAction::MortarRequested:
                event = SpecialMoveEvent::MortarRequested;
                break;
            case SpecialAction::RollLeftRequested:
                event = SpecialMoveEvent::RollLeftRequested;
                break;
            case SpecialAction::RollRightRequested:
                event = SpecialMoveEvent::RollRightRequested;
                break;
            default:
                break;
            }
            return stateMachine.Update(event, mobilityStopped);
        }

        void Reset() { m_recognizer.Reset(); }

    private:
        SpecialActionRecognizer m_recognizer;
    };
}
