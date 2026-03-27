#pragma once
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <iostream>

namespace Aura::Core::Engine {

/**
 * @struct UndoAction
 * @brief Lambda-based Command Pattern for professional DAW undo/redo.
 */
struct UndoAction {
    std::string name;
    std::function<void()> undo;
    std::function<void()> redo;
};

/**
 * @class UndoTransactionManager
 * @brief THE ARCHIVIST: Manages the professional history stack.
 * SOLVES: "Hollow architecture" and "UX instability" from the audit.
 */
class UndoTransactionManager {
public:
    static UndoTransactionManager& getInstance() {
        static UndoTransactionManager instance;
        return instance;
    }

    void performAction(const std::string& name, std::function<void()> undo, std::function<void()> redo) {
        redo(); // Execute the action first
        m_undoStack.push_back({ name, undo, redo });
        m_redoStack.clear(); // Clear the future
        if (m_undoStack.size() > kMaxHistory) m_undoStack.erase(m_undoStack.begin());
        std::cout << "[Undo] Action Performed: " << name << std::endl;
    }

    void undo() {
        if (m_undoStack.empty()) return;
        auto action = m_undoStack.back();
        action.undo();
        m_redoStack.push_back(action);
        m_undoStack.pop_back();
        std::cout << "[Undo] Reverted: " << action.name << std::endl;
    }

    void redo() {
        if (m_redoStack.empty()) return;
        auto action = m_redoStack.back();
        action.redo();
        m_undoStack.push_back(action);
        m_redoStack.pop_back();
        std::cout << "[Redo] Restored: " << action.name << std::endl;
    }

private:
    static constexpr size_t kMaxHistory = 100;
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
};

} // namespace Aura::Core::Engine
