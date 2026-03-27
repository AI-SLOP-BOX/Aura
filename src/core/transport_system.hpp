#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include "engine/automation_curve.hpp"

namespace Aura::Core {

/**
 * @brief TransportSystem: High-precision time-base for the DAW.
 * Calculates Beats, Bars, and Samples with sample-accurate precision.
 */
class TransportSystem {
public:
    struct Position {
        uint32_t bar = 1;
        uint32_t beat = 1;
        double tick = 0.0;
        uint64_t totalSamples = 0;
        
        struct Trigger {
            bool isNewBeat = false;
            bool isNewBar = false;
        } trigger;
    };

    TransportSystem(double sr) : m_sampleRate(sr) {
        m_tempoCurve = std::make_unique<Engine::AutomationCurve>();
        m_tempoCurve->addPoint(0, 120.0f); // Default 120 BPM
    }

    void setBPM(double bpm) { m_bpm.store(bpm); }
    double getBPM() const { return m_bpm.load(); }
    void togglePlay(bool play) { m_isPlaying.store(play); }

    /**
     * @brief ACCELERATED TEMPO: Retrieves BPM from the automation curve at current playhead.
     */
    void updateBPMFromCurve() {
        if (m_tempoCurve) {
            m_bpm.store(m_tempoCurve->getValueAt(m_totalSamples));
        }
    }

    /**
     * @brief Updates the transport by a block size.
     * HONEST FIX: Returns trigger flags for metronome sync.
     */
    Position::Trigger advance(uint32_t numSamples) {
        Position::Trigger trigger;
        if (!m_isPlaying.load()) return trigger;

        // Dynamic BPM update
        updateBPMFromCurve();

        double samplesPerBeat = (60.0 / m_bpm.load()) * m_sampleRate;
        uint64_t prevBeatInt = static_cast<uint64_t>(m_currentBeat);
        
        m_totalSamples += numSamples;
        m_currentBeat = static_cast<double>(m_totalSamples) / samplesPerBeat;
        
        uint64_t currBeatInt = static_cast<uint64_t>(m_currentBeat);
        if (currBeatInt > prevBeatInt) {
            trigger.isNewBeat = true;
            if (currBeatInt % 4 == 0) trigger.isNewBar = true;
        }
        
        return trigger;
    }

    Position getPosition() const {
        Position p;
        p.totalSamples = m_totalSamples;
        p.beat = static_cast<uint32_t>(m_currentBeat) % 4 + 1;
        p.bar = static_cast<uint32_t>(m_currentBeat) / 4 + 1;
        return p;
    }

private:
    double m_sampleRate;
    std::atomic<double> m_bpm{120.0};
    std::atomic<bool> m_isPlaying{false};
    uint64_t m_totalSamples = 0;
    double m_currentBeat = 0.0;
    std::unique_ptr<Engine::AutomationCurve> m_tempoCurve;
};

} // namespace Aura::Core
