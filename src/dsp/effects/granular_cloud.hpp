#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include "../iprocessor.hpp"
#include "../utils/dsp_utils.hpp"

namespace Aura::DSP::Effects {

/**
 * @class GranularCloud
 * @brief Professional Granular Synthesis for Ambient and Ethereal textures.
 */
class GranularCloud : public IProcessor {
public:
    static constexpr size_t kMaxGrains = 16;
    static constexpr size_t kWindowSize = 1024;

    struct Grain {
        double currentPos = 0.0;
        uint32_t length = 0;
        float pitch = 1.0f;
        float env = 0.0f;
        bool active = false;
    };

    GranularCloud() : m_writeIdx(0) {
        // Pre-initialize window table (Hanning)
        for (size_t i = 0; i < kWindowSize; ++i) {
            m_windowTable[i] = 0.5f * (1.0f - std::cos(Utils::DSPUtils::TWO_PI * i / kWindowSize));
        }
        m_grains.resize(kMaxGrains);
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        // Pre-allocate buffer for 2 seconds (No resize in audio thread)
        uint32_t samples = static_cast<uint32_t>(sr * 2.0);
        if (m_circBufferL.size() < samples) {
            m_circBufferL.assign(samples, 0.0f);
            m_circBufferR.assign(samples, 0.0f);
        }
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed || m_circBufferL.empty()) return;

        uint32_t numSamples = buffer.getNumSamples();
        uint32_t bufSize = static_cast<uint32_t>(m_circBufferL.size());
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Record into Circular Buffer
            m_circBufferL[m_writeIdx] = buffer.getReadPointer(0)[s];
            m_circBufferR[m_writeIdx] = buffer.getReadPointer(1)[s];

            // 2. Spawn Grains (Density-based)
            if (fastRand() < m_density * 42949672.0f) spawnGrain(); // fastRand is 0..UINT32_MAX

            // 3. Render Grains
            float outL = 0.0f, outR = 0.0f;
            for (auto& g : m_grains) {
                if (!g.active) continue;

                // Linear Interpolation
                uint32_t p0 = static_cast<uint32_t>(g.currentPos);
                uint32_t p1 = (p0 + 1) % bufSize;
                float frac = static_cast<float>(g.currentPos - p0);

                outL += (m_circBufferL[p0] + frac * (m_circBufferL[p1] - m_circBufferL[p0])) * g.env;
                outR += (m_circBufferR[p0] + frac * (m_circBufferR[p1] - m_circBufferR[p0])) * g.env;

                g.currentPos = std::fmod(g.currentPos + g.pitch, static_cast<double>(bufSize));
                
                // Optimized Window Lookup
                m_grainAge[(&g - &m_grains[0])]++;
                if (m_grainAge[(&g - &m_grains[0])] >= g.length) {
                    g.active = false;
                } else {
                    float t = static_cast<float>(m_grainAge[(&g - &m_grains[0])]) / g.length;
                    g.env = m_windowTable[static_cast<uint32_t>(t * (kWindowSize - 1))];
                }
            }

            // Mix with smoothed mix (IProcessor::m_mix is handled by UI)
            float dryL = buffer.getReadPointer(0)[s];
            float dryR = buffer.getReadPointer(1)[s];
            
            buffer.getWritePointer(0)[s] = dryL * (1.0f - m_mix) + (outL * 0.2f) * m_mix;
            buffer.getWritePointer(1)[s] = dryR * (1.0f - m_mix) + (outR * 0.2f) * m_mix;

            m_writeIdx = (m_writeIdx + 1) % bufSize;
        }
    }

    void setParameter(uint32_t id, float value) noexcept override {
        if (id == 0) m_mix = value;
        else if (id == 1) m_density = std::clamp(value, 0.0f, 1.0f);
    }

    void reset() noexcept override {
        std::fill(m_circBufferL.begin(), m_circBufferL.end(), 0.0f);
        std::fill(m_circBufferR.begin(), m_circBufferR.end(), 0.0f);
        for (auto& g : m_grains) g.active = false;
        m_grainAge.fill(0);
        m_writeIdx = 0;
    }

private:
    void spawnGrain() {
        for (uint32_t i = 0; i < kMaxGrains; ++i) {
            auto& g = m_grains[i];
            if (!g.active) {
                g.active = true;
                uint32_t bufSize = static_cast<uint32_t>(m_circBufferL.size());
                uint32_t offset = (fastRand() % 44100); // Up to 1sec ago
                g.currentPos = (m_writeIdx + bufSize - offset) % bufSize;
                g.length = 2000 + (fastRand() % 8000);
                g.pitch = 0.5f + (fastRand() % 1500) / 1000.0f;
                m_grainAge[i] = 0;
                return;
            }
        }
    }

    inline uint32_t fastRand() {
        m_rngState ^= (m_rngState << 13);
        m_rngState ^= (m_rngState >> 17);
        m_rngState ^= (m_rngState << 5);
        return m_rngState;
    }

    double m_sampleRate = 44100.0;
    std::vector<float> m_circBufferL, m_circBufferR;
    std::vector<Grain> m_grains;
    std::array<uint32_t, kMaxGrains> m_grainAge;
    std::array<float, kWindowSize> m_windowTable;
    uint32_t m_writeIdx = 0;
    uint32_t m_rngState = 0xACE1;
    float m_density = 0.2f;
};

} // namespace Aura::DSP::Effects
