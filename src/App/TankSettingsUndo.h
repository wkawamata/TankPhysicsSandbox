#pragma once

#include "SnapshotUndoEdit.h"
#include "Physics/TankTypes.h"

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
            m_edit.Update(before, m_settings, interaction, m_history, Apply(), "Tank parameter edit");
        }

        void FinishInteraction(std::uint64_t interaction)
        {
            m_edit.FinishInteraction(interaction, m_history, Apply(), "Tank parameter edit");
        }

        void Flush() { m_edit.Flush(m_history, Apply(), "Tank parameter edit"); }
        bool CanUndo() const { return m_edit.HasPending() || m_history.CanUndo(); }
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

        void Clear() { m_edit.Clear(); m_history.Clear(); }

    private:
        SnapshotUndoEdit<Physics::TankSettings>::Apply Apply()
        {
            return [settings = &m_settings](const auto& state) { *settings = state; };
        }

        Physics::TankSettings& m_settings;
        UndoHistory m_history;
        SnapshotUndoEdit<Physics::TankSettings> m_edit;
    };
}
