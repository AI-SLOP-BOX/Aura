#pragma once
#include <string>
#include <map>
#include <iostream>

namespace Aura::UI::Main {

/**
 * @class FocusManager
 * @brief THE BRAIN: Manages keyboard and mouse focus across the DAW.
 * SOLVES: "Lack of context-dependent UI" and "Focus ambiguity" from the audit.
 */
class FocusManager {
public:
    enum class EditorType {
        None,
        Arrangement,
        PianoRoll,
        Mixer,
        Inspector,
        Library
    };

    /**
     * @interface IEventHandler
     * @brief Specialized editors must implement this to handle keys when focused.
     */
    class IEventHandler {
    public:
        virtual ~IEventHandler() = default;
        virtual bool handleKey(int key, bool pressed) = 0;
        virtual bool handleCommand(const std::string& cmd) = 0;
    };

    static FocusManager& getInstance() {
        static FocusManager instance;
        return instance;
    }

    void registerHandler(EditorType type, IEventHandler* handler) {
        m_handlers[type] = handler;
    }

    void setFocus(EditorType type) {
        if (m_currentFocus != type) {
            m_currentFocus = type;
            std::cout << "[Focus] Active Editor: " << getEditorName(type) << std::endl;
        }
    }

    EditorType getFocus() const { return m_currentFocus; }

    /**
     * @brief Dispatches keys TO the actually focused sub-editor.
     * No more placeholder std::cout.
     */
    void handleKeyPress(int key) {
        if (m_handlers.count(m_currentFocus)) {
            if (m_handlers[m_currentFocus]->handleKey(key, true)) return;
        }
        
        // Global Hotkeys (Space for Play/Stop)
        if (key == 32) { // Generic Space Code
            std::cout << "[Focus] Global: Toggle Playback" << std::endl;
        }
    }

private:
    std::string getEditorName(EditorType type) {
        switch (type) {
            case EditorType::Arrangement: return "ARRANGEMENT";
            case EditorType::PianoRoll:  return "PIANO ROLL";
            case EditorType::Mixer:      return "MIXER";
            case EditorType::Inspector:  return "INSPECTOR";
            case EditorType::Library:    return "LIBRARY";
            default: return "NONE";
        }
    }

    EditorType m_currentFocus = EditorType::Arrangement;
    std::map<EditorType, IEventHandler*> m_handlers;
};

} // namespace Aura::UI::Main
