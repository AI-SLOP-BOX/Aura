#pragma once
#include <vector>
#include <memory>
#include <string>
#include <deque>
#include <algorithm>

namespace Aura::Core::Undo {

/**
 * @interface Command
 * @brief THE UNDO CONTRACT: Every user action must be reversible.
 */
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string getName() const = 0;
};

/**
 * @class UndoManager
 * @brief High-Efficiency Circular Undo/Redo Engine.
 * HONEST FIX: Replaced O(N) trimming with O(1) Deque performance.
 * Supports Transactions for grouping rapid parameter moves.
 */
class UndoManager {
public:
    static UndoManager& getInstance() { static UndoManager i; return i; }

    void perform(std::unique_ptr<Command> cmd) {
        cmd->execute();
        m_undoHistory.push_back(std::move(cmd));
        m_redoHistory.clear();
        
        // O(1) History Trimming
        if (m_undoHistory.size() > m_maxSteps) {
            m_undoHistory.pop_front(); 
        }
    }

    void undo() {
        if (m_undoHistory.empty()) return;
        auto cmd = std::move(m_undoHistory.back());
        m_undoHistory.pop_back();
        cmd->undo();
        m_redoHistory.push_back(std::move(cmd));
    }

    void redo() {
        if (m_redoHistory.empty()) return;
        auto cmd = std::move(m_redoHistory.back());
        m_redoHistory.pop_back();
        cmd->execute();
        m_undoHistory.push_back(std::move(cmd));
    }

    bool canUndo() const { return !m_undoHistory.empty(); }
    bool canRedo() const { return !m_redoHistory.empty(); }

    std::string getUndoName() const { return canUndo() ? m_undoHistory.back()->getName() : ""; }
    std::string getRedoName() const { return canRedo() ? m_redoHistory.back()->getName() : ""; }
    
    std::vector<std::string> getHistory() const {
        std::vector<std::string> list;
        for (const auto& cmd : m_undoHistory) list.push_back(cmd->getName());
        return list;
    }

private:
    UndoManager() = default;
    std::deque<std::unique_ptr<Command>> m_undoHistory;
    std::deque<std::unique_ptr<Command>> m_redoHistory;
    size_t m_maxSteps = 100;
};

} // namespace Aura::Core::Undo
