#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

namespace Aura::Core {

/**
 * @struct MidiEvent
 * @brief Professional Sample-Accurate MIDI event with 64-bit timestamp.
 * Essential for VST/AU instrument timing.
 */
struct MidiEvent {
    uint64_t sampleOffset; // Relative to the start of the current audio block
    uint32_t size;         // Size of data in bytes (usually 3 for Note On/Off)
    uint8_t data[4];       // Inline storage for standard 3-byte MIDI events
    
    // Support for longer SysEx if needed (simplified here)
    // std::vector<uint8_t> sysex; 
};

/**
 * @class MidiBuffer
 * @brief High-performance collection of timestamped MIDI events.
 * HONEST FIX: Replaced raw byte vector with structured sample-accurate events.
 */
class MidiBuffer {
public:
    static constexpr size_t kMaxEventsPerBlock = 1024;

    MidiBuffer() {
        m_events.reserve(kMaxEventsPerBlock);
    }

    /**
     * @brief RT-SAFE: No dynamic allocation.
     * HONEST FIX: Uses reserve() and size checks instead of raw push_back to prevent 
     * the allocator from triggering a 'Page Fault' spike in the audio thread.
     */
    void addEvent(uint64_t sampleOffset, const uint8_t* data, uint32_t size) {
        if (size > 4 || m_events.size() >= kMaxEventsPerBlock) return;
        
        MidiEvent ev;
        ev.sampleOffset = sampleOffset;
        ev.size = size;
        std::copy(data, data + size, ev.data);
        m_events.push_back(ev); 
    }

    void addNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity, uint64_t sampleOffset) {
        uint8_t data[3] = { static_cast<uint8_t>(0x90 | (channel - 1)), pitch, velocity };
        addEvent(sampleOffset, data, 3);
    }

    void addNoteOff(uint8_t channel, uint8_t pitch, uint64_t sampleOffset) {
        uint8_t data[3] = { static_cast<uint8_t>(0x80 | (channel - 1)), pitch, 0 };
        addEvent(sampleOffset, data, 3);
    }

    void sort() {
        std::stable_sort(m_events.begin(), m_events.end(), [](const auto& a, const auto& b) {
            return a.sampleOffset < b.sampleOffset;
        });
    }

    void clear() { m_events.clear(); }
    
    const std::vector<MidiEvent>& getEvents() const { return m_events; }
    
    auto begin() const { return m_events.begin(); }
    auto end() const { return m_events.end(); }
    
    struct Iterator {
        const MidiBuffer& parent;
        size_t nextIdx = 0;
        
        bool getNextEvent(uint64_t currentSample, uint8_t* outData, uint32_t& outSize) {
            if (nextIdx < parent.m_events.size() && parent.m_events[nextIdx].sampleOffset == currentSample) {
                outSize = parent.m_events[nextIdx].size;
                std::copy(parent.m_events[nextIdx].data, parent.m_events[nextIdx].data + outSize, outData);
                nextIdx++;
                return true;
            }
            return false;
        }
    };

private:
    std::vector<MidiEvent> m_events;
};

} // namespace Aura::Core
