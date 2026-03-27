#pragma once

#include <vector>
#include <string>
#include <array>
#include <algorithm>

namespace Aura::Core::Engine {

/**
 * @brief ScaleSystem: High-end Music Theory Intelligence.
 * Standard for modern DAWs (Logic 11, Studio One).
 */
class ScaleSystem {
public:
    enum class Type { Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian };

    struct Scale {
        int root; // 0=C, 1=C#, etc.
        Type type;
        std::array<bool, 12> pattern;
    };

    static ScaleSystem& getInstance() { static ScaleSystem i; return i; }

    void setScale(int root, Type type) {
        m_activeScale.root = root;
        m_activeScale.type = type;
        m_activeScale.pattern = getPattern(type);
    }

    /**
     * @brief SNAP-TO-KEY: Force a MIDI note into the active scale.
     */
    int quantizeNote(int note) {
        int pc = note % 12;
        int octave = note / 12;
        
        int relativePc = (pc - m_activeScale.root + 12) % 12;
        if (m_activeScale.pattern[relativePc]) return note;

        // Find nearest scale degree
        for (int i = 1; i < 6; ++i) {
            if (m_activeScale.pattern[(relativePc + i) % 12]) return (octave * 12) + ((pc + i) % 12);
            if (m_activeScale.pattern[(relativePc - i + 12) % 12]) return (octave * 12) + ((pc - i + 12) % 12);
        }
        return note;
    }

    const Scale& getActiveScale() const { return m_activeScale; }

    struct Chord {
        int root; // 0=C, 1=C#, etc.
        std::vector<int> intervals; // [0, 4, 7] for Major
        std::string name;
    };

    struct ChordEvent {
        double beat;
        Chord chord;
    };

    void addChord(double beat, int root, const std::vector<int>& intervals, const std::string& name) {
        m_chordTrack.push_back({beat, {root, intervals, name}});
        std::sort(m_chordTrack.begin(), m_chordTrack.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    }

    Chord getChordAt(double beat) {
        if (m_chordTrack.empty()) return {m_activeScale.root, {0, 4, 7}, "M"};
        auto it = std::upper_bound(m_chordTrack.begin(), m_chordTrack.end(), beat, [](double b, const auto& ev) { return b < ev.beat; });
        if (it == m_chordTrack.begin()) return m_chordTrack[0].chord;
        return std::prev(it)->chord;
    }

private:
    ScaleSystem() { 
        setScale(0, Type::Major); 
        // Default I-IV-V-I progression
        addChord(0, 0, {0, 4, 7}, "C");
        addChord(4, 5, {0, 4, 7}, "F");
        addChord(8, 7, {0, 4, 7}, "G");
        addChord(12, 0, {0, 4, 7}, "C");
    }

    std::vector<ChordEvent> m_chordTrack;
    Scale m_activeScale;

    std::array<bool, 12> getPattern(Type t) {
        switch (t) {
            case Type::Major:      return {true, false, true, false, true, true, false, true, false, true, false, true};
            case Type::Minor:      return {true, false, true, true, false, true, false, true, true, false, true, false};
            case Type::Dorian:     return {true, false, true, true, false, true, false, true, false, true, true, false};
            case Type::Phrygian:   return {true, true, false, true, false, true, false, true, true, false, true, false};
            case Type::Lydian:     return {true, false, true, false, true, false, true, true, false, true, false, true};
            case Type::Mixolydian: return {true, false, true, false, true, true, false, true, false, true, true, false};
            case Type::Locrian:    return {true, true, false, true, false, true, true, false, true, false, true, false};
            default:               return {true, false, true, false, true, true, false, true, false, true, false, true};
        }
    }
};

} // namespace Aura::Core::Engine
