#pragma once

#include <vector>
#include <map>
#include <cmath>

namespace Aura::Core::Engine {

/**
 * @brief PitchBlock: A single sung note to be corrected.
 * Foundation for 'Graphic Pitch Correction' (Melodyne-style).
 */
struct PitchBlock {
    uint64_t startSample;
    uint64_t endSample;
    float targetNote;     // Chromatic target (e.g. 60.0 = C4)
    float vibratoAmount;  // [0, 1] Scaling of natural vibrato
    float driftAmount;    // [0, 1] Smoothing of pitch slide
};

/**
 * @brief VocalPitchEditor: Professional surgical tuning engine.
 * Essential for the 'High-End Vocal' sound.
 */
class VocalPitchEditor {
public:
    static VocalPitchEditor& getInstance() { static VocalPitchEditor i; return i; }

    void addBlock(uint64_t start, uint64_t end, float target) {
        m_blocks.push_back({ start, end, target, 1.0f, 1.0f });
    }

    /**
     * @brief ACCURATE PITCH SHIFT: Calculates the required shift for a sample.
     * @param detectedFreq: Current raw freq from the audio.
     */
    float getShiftRatio(uint64_t now, float detectedFreq) const {
        const PitchBlock* block = findBlock(now);
        if (!block) return 1.0f;

        float targetFreq = 440.0f * std::pow(2.0f, (block->targetNote - 69.0f) / 12.0f);
        
        // (Conceptual algorithm to blend detected with target based on drift/vibrato settings)
        float diff = targetFreq / detectedFreq;
        return diff;
    }

private:
    VocalPitchEditor() = default;

    const PitchBlock* findBlock(uint64_t now) const {
        for (const auto& b : m_blocks) {
            if (now >= b.startSample && now < b.endSample) return &b;
        }
        return nullptr;
    }

    std::vector<PitchBlock> m_blocks;
};

} // namespace Aura::Core::Engine
