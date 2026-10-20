#pragma once

#include "SnapshotUndoEdit.h"
#include "Physics/TankTypes.h"
#include <any>
#include <typeindex>

namespace Tank::App
{
    // Owns editing history only. Applying physics settings remains an explicit action.
    class TankSettingsUndo
    {
    public:
        explicit TankSettingsUndo(Physics::TankSettings& settings) : m_settings(settings) {}
        TankSettingsUndo(const TankSettingsUndo&) = delete;
        TankSettingsUndo& operator=(const TankSettingsUndo&) = delete;

        void Observe(const Physics::TankSettings& before, std::uint64_t interaction)
        {
            if (!(before == m_settings)) FlushExternal();
            m_edit.Update(before, m_settings, interaction, m_history, Apply(), "Tank parameter edit");
        }

        void FinishInteraction(std::uint64_t interaction)
        {
            if (m_externalBefore.has_value() && m_externalInteraction != interaction) FlushExternal();
            m_edit.FinishInteraction(interaction, m_history, Apply(), "Tank parameter edit");
        }

        // External editor values share the tank history, preserving edit order.
        template<class State, class ApplyState>
        void ObserveExternal(const State& before, const State& after,
            std::uint64_t interaction, ApplyState apply, const char* name)
        {
            if (before == after) return;
            m_edit.Flush(m_history, Apply(), "Tank parameter edit");
            if (m_externalBefore.has_value() &&
                (m_externalInteraction != interaction || m_externalType != typeid(State)))
                FlushExternal();
            if (!m_externalBefore.has_value()) m_externalBefore = before;
            m_externalInteraction = interaction;
            m_externalType = typeid(State);
            const auto original = std::any_cast<State>(m_externalBefore);
            m_externalRecord = [this, original, after, apply, name]
            {
                if (original == after) return;
                m_history.RecordApplied(std::make_unique<LambdaUndoCommand>(name,
                    [apply, original] { apply(original); },
                    [apply, after] { apply(after); }));
            };
            if (interaction == 0) FlushExternal();
        }

        void Flush() { m_edit.Flush(m_history, Apply(), "Tank parameter edit"); FlushExternal(); }
        bool CanUndo() const { return m_edit.HasPending() || m_externalBefore.has_value() || m_history.CanUndo(); }
        bool CanRedo() const { return m_history.CanRedo(); }
        size_t UndoCount() const { return m_history.UndoCount(); }

        bool Undo()
        {
            Flush();
            if (!m_history.CanUndo()) return false;
            m_history.Undo();
            return true;
        }

        bool Redo()
        {
            Flush();
            if (!m_history.CanRedo()) return false;
            m_history.Redo();
            return true;
        }

        void Clear() { m_edit.Clear(); m_externalBefore.reset(); m_externalRecord = {}; m_history.Clear(); }

    private:
        void FlushExternal()
        {
            if (m_externalRecord) m_externalRecord();
            m_externalRecord = {};
            m_externalBefore.reset();
        }
        std::any m_externalBefore;
        std::type_index m_externalType{typeid(void)};
        std::uint64_t m_externalInteraction = 0;
        std::function<void()> m_externalRecord;
        SnapshotUndoEdit<Physics::TankSettings>::Apply Apply()
        {
            return [settings = &m_settings](const auto& state) { *settings = state; };
        }

        Physics::TankSettings& m_settings;
        UndoHistory m_history;
        SnapshotUndoEdit<Physics::TankSettings> m_edit;
    };
}
