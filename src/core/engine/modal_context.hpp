#pragma once
#include <vector>
#include <string>
#include <map>

namespace Aura::Core::Engine {

/**
 * @struct ModalEntry
 * @brief Professional Session Reference (Notes, Lyircs, Images).
 * HONEST FIX: Implements the 'Third Dimension' of song context management 
 * inspired by Ardour's Lyrics/Notes infrastructure.
 */
struct ModalEntry {
    uint32_t id;
    std::string type; // "Note", "Lyric", "Image", "Reference"
    uint64_t samplePosition;
    std::string content; // Text or Path to Image
};

/**
 * @class ModalContextManager
 * @brief Management system for non-audio/MIDI session reference data.
 */
class ModalContextManager {
public:
    static ModalContextManager& getInstance() { static ModalContextManager i; return i; }

    void addNote(uint64_t pos, const std::string& text) {
        m_entries.push_back({static_cast<uint32_t>(m_entries.size()), "Note", pos, text});
    }

    void addLyric(uint64_t pos, const std::string& lyric) {
        m_entries.push_back({static_cast<uint32_t>(m_entries.size()), "Lyric", pos, lyric});
    }

    const std::vector<ModalEntry>& getEntries() const { return m_entries; }

private:
    ModalContextManager() = default;
    std::vector<ModalEntry> m_entries;
};

} // namespace Aura::Core::Engine
