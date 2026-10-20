#pragma once

#include "UndoHistory.h"
#include <cstdint>
#include <optional>

namespace Tank::App
{
    // Groups frames of one widget interaction, without requiring valid serialized data.
    template<class State, class Equal = std::equal_to<State>>
    class SnapshotUndoEdit
    {
    public:
        using Apply = std::function<void(const State&)>;

        void Update(const State& before, const State& after, std::uint64_t interaction,
            UndoHistory& history, const Apply& apply, const char* name)
        {
            FinishInteraction(interaction, history, apply, name);
            if (!Equal{}(before, after))
            {
                if (!m_before) m_before = before;
                m_after = after;
                m_interaction = interaction;
            }
            if (interaction == 0) Flush(history, apply, name);
        }

        void FinishInteraction(std::uint64_t interaction, UndoHistory& history,
            const Apply& apply, const char* name)
        {
            if (m_before && m_interaction != interaction) Flush(history, apply, name);
        }

        void Flush(UndoHistory& history, const Apply& apply, const char* name)
        {
            if (!m_before) return;
            if (!Equal{}(*m_before, *m_after))
            {
                history.RecordApplied(std::make_unique<LambdaUndoCommand>(name,
                    [apply, state = *m_before] { apply(state); },
                    [apply, state = *m_after] { apply(state); }));
            }
            Clear();
        }

        bool HasPending() const { return m_before.has_value(); }
        void Clear() { m_before.reset(); m_after.reset(); m_interaction = 0; }

    private:
        std::optional<State> m_before;
        std::optional<State> m_after;
        std::uint64_t m_interaction = 0;
    };
}
