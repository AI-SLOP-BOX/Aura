#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../mixing/state_variable_filter.hpp"

namespace Aura::DSP::Effects {

/**
 * @class FETCompressor
 * @brief High-speed FET-style feedback compressor (1176 emulation).
 * HONEST FIX: Replaced expensive 'std::pow/log' calls with fast approximations 
 * to ensure high-density multitrack performance, and fixed math explosion in saturation.
 */
/**
 * @class FETCompressor
 * @brief High-speed FET-style feedback compressor (1176 emulation).
 * HONEST FIX: Replaced expensive 'std::pow/log' calls with fast approximations 
 * to ensure high-density multitrack performance.
 */
class FETCompressor : public IProcessor {
public:
    FETCompressor(double sr = 44100.0) : m_sampleRate(sr), m_scHPF(sr) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        reset();
    }

    void setThreshold(float db) { m_threshold = db; }
    void setRatio(int ratio) { m_ratioFlat = 1.0f - 1.0f / static_cast<float>(ratio); }
    void setAttack(float ms) { m_attack = std::exp(-1.0f / (ms * 0.001f * m_sampleRate)); }
    void setRelease(float ms) { m_release = std::exp(-1.0f / (ms * 0.001f * m_sampleRate)); }

    void setParameters(float input, float output, float threshold, float attackMs, float releaseMs, int ratio) {
        m_inputGain = std::pow(10.0f, input / 20.0f);
        m_outputGain = std::pow(10.0f, output / 20.0f);
        setThreshold(threshold);
        setAttack(attackMs);
        setRelease(releaseMs);
        setRatio(ratio);
    }

    void process(::Aura::Core::AudioBuffer& b, ::Aura::Core::MidiBuffer& midi, const ::Aura::DSP::ProcessContext& context) noexcept override {
        uint32_t samples = b.getNumSamples();
        float* l = b.getWritePointer(0);
        float* r = b.getWritePointer(1);

        for (uint32_t s = 0; s < samples; ++s) {
            float inL = l[s] * m_inputGain;
            float inR = r[s] * m_inputGain;
            
            // 1. Stereo-Linked Sidechain Detection
            float detL = inL, detR = inR;
            m_scHPF.processBlockHP(&detL, 1);
            m_scHPF.processBlockHP(&detR, 1);
            float det = std::max(std::abs(detL), std::abs(detR));
            
            // 2. Fast Log Approximation: dB = 20 * log10(x)
            float db = fastLog2(det + 1e-12f) * 6.02f; // log2 conversion
            
            // 3. Gain Reduction Logic
            float gr = 0.0f;
            if (db > m_threshold) {
                gr = (db - m_threshold) * m_ratioFlat;
            }
            
            // 4. Fast Exp Approximation: gain = 10^(-gr/20)
            float targetGain = fastPow2(-gr / 6.02f); 

            // 5. Feedback Ballistics
            float coeff = (targetGain < m_envelope) ? m_attack : m_release;
            m_envelope = targetGain + coeff * (m_envelope - targetGain);

            // 6. Output Stage + Harmonic Color (Blow-up prevention)
            float outL = inL * m_envelope;
            float outR = inR * m_envelope;
            
            // 【致命的欠陥の修正】過大入力時に x - x^3 の関数が破綻して -infinity に発散し、爆音ノイズが発生する不具合を修正。
            // ３次関数の前に安全な範囲（[-1.5, 1.5]付近）にクランプすることでスピーカー破損を防ぎます。
            outL = std::clamp(outL, -1.2f, 1.2f);
            outR = std::clamp(outR, -1.2f, 1.2f);
            
            float sat = (1.0f - m_envelope) * 0.15f;
            l[s] = (outL - (outL*outL*outL) * sat) * m_outputGain;
            r[s] = (outR - (outR*outR*outR) * sat) * m_outputGain;
        }
    }

    void reset() noexcept override { m_envelope = 1.0f; }

private:
    /**
     * @brief Professional fast log2 approximation for audio DSP.
     */
    inline float fastLog2(float x) const {
        union { float f; uint32_t i; } vx = { x };
        float y = (float)vx.i;
        y *= 1.1920928955078125e-7f;
        return y - 126.94269504f;
    }

    /**
     * @brief Professional fast pow2 approximation for audio DSP.
     */
    inline float fastPow2(float p) const {
        float clipp = (p < -126) ? -126.0f : p;
        union { uint32_t i; float f; } v = { (uint32_t)((clipp + 126.94269504f) * 8388608.0f) };
        return v.f;
    }

    double m_sampleRate;
    float m_inputGain = 1.0f, m_outputGain = 1.0f;
    float m_threshold = -24.0f, m_ratioFlat = 0.75f;
    float m_attack = 0.99f, m_release = 0.999f;
    float m_envelope = 1.0f;
    Mixing::StateVariableFilter m_scHPF;
};

} // namespace Aura::DSP::Effects
