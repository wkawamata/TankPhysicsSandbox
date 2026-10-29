#pragma once

#include "App/UndoHistory.h"

namespace Tank::App
{
    // Record one already-applied checkbox change without restoring unrelated windows.
    inline void RecordUiVisibilityChange(UndoHistory& history, bool& visible, bool previous)
    {
        if (visible == previous) return;
        bool* const field = &visible;
        const bool current = visible;
        history.RecordApplied(std::make_unique<LambdaUndoCommand>("Window visibility",
            [field, previous] { *field = previous; },
            [field, current] { *field = current; }));
    }
}
