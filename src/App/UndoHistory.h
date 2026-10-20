#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Tank::App
{
    class IUndoCommand
    {
    public:
        virtual ~IUndoCommand() = default;
        virtual const std::string& Name() const = 0;
        virtual void Undo() = 0;
        virtual void Redo() = 0;
    };

    class UndoHistory
    {
    public:
        void Execute(std::unique_ptr<IUndoCommand> command)
        {
            if (!command) return;
            command->Redo();
            RecordApplied(std::move(command));
        }

        // For edits already applied by an immediate-mode UI.
        void RecordApplied(std::unique_ptr<IUndoCommand> command)
        {
            if (!command) return;
            if (m_cursor < m_commands.size())
                m_commands.erase(m_commands.begin() + static_cast<std::ptrdiff_t>(m_cursor), m_commands.end());
            m_commands.push_back(std::move(command));
            m_cursor = m_commands.size();
        }

        bool CanUndo() const { return m_cursor != 0; }
        bool CanRedo() const { return m_cursor < m_commands.size(); }
        size_t UndoCount() const { return m_cursor; }
        size_t RedoCount() const { return m_commands.size() - m_cursor; }

        std::string Undo()
        {
            if (!CanUndo()) return {};
            --m_cursor;
            const std::string name = m_commands[m_cursor]->Name();
            m_commands[m_cursor]->Undo();
            return name;
        }

        std::string Redo()
        {
            if (!CanRedo()) return {};
            const std::string name = m_commands[m_cursor]->Name();
            m_commands[m_cursor]->Redo();
            ++m_cursor;
            return name;
        }

        void Clear()
        {
            m_commands.clear();
            m_cursor = 0;
        }

    private:
        std::vector<std::unique_ptr<IUndoCommand>> m_commands;
        size_t m_cursor = 0;
    };

    class LambdaUndoCommand final : public IUndoCommand
    {
    public:
        LambdaUndoCommand(std::string name, std::function<void()> undo, std::function<void()> redo)
            : m_name(std::move(name)), m_undo(std::move(undo)), m_redo(std::move(redo)) {}

        const std::string& Name() const override { return m_name; }
        void Undo() override { m_undo(); }
        void Redo() override { m_redo(); }

    private:
        std::string m_name;
        std::function<void()> m_undo;
        std::function<void()> m_redo;
    };
}
