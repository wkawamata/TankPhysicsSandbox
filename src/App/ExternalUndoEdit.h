#pragma once
#include "UndoHistory.h"
#include <any>
#include <cstdint>
#include <typeindex>

namespace Tank::App
{
    // Groups typed editor snapshots in an existing shared command history.
    class ExternalUndoEdit
    {
    public:
        template<class State, class Apply>
        void Update(const State& before, const State& after, std::uint64_t interaction,
            UndoHistory& history, Apply apply, const char* name)
        {
            FinishInteraction(interaction);
            if (before == after) return;
            if (m_before.has_value() && m_type != typeid(State)) Flush();
            if (!m_before.has_value()) m_before = before;
            m_type = typeid(State);
            m_interaction = interaction;
            const auto original = std::any_cast<State>(m_before);
            m_record = [&history, original, after, apply, name]
            {
                if (original == after) return;
                history.RecordApplied(std::make_unique<LambdaUndoCommand>(name,
                    [apply, original] { apply(original); }, [apply, after] { apply(after); }));
            };
            if (!interaction) Flush();
        }
        void FinishInteraction(std::uint64_t interaction)
        { if (m_before.has_value() && m_interaction != interaction) Flush(); }
        void Flush() { if (m_record) m_record(); Clear(); }
        void Clear() { m_record = {}; m_before.reset(); }
        bool HasPending() const { return m_before.has_value(); }
    private:
        std::any m_before;
        std::type_index m_type{typeid(void)};
        std::uint64_t m_interaction = 0;
        std::function<void()> m_record;
    };
}
