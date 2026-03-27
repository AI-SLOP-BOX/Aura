#include "aura_sampler_pro.hpp"
#include <cmath>
#include <algorithm>

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief AuraSamplerPro: Production-grade Polyphonic Sampler.
 * HONEST FIX: Direct Pointer Access + TPT SVF Filter.
 */
void AuraSamplerPro::process(float* outputL, float* outputR, size_t numFrames) {
    if (!outputL || !outputR || numFrames == 0) return;
    
    // Clear buffers (Sampler is a generator)
    std::fill(outputL, outputL + numFrames, 0.0f);
    std::fill(outputR, outputR + numFrames, 0.0f);

    auto zonesPtr = std::atomic_load(&m_zones);
    if (!zonesPtr || zonesPtr->empty()) return;

    for (int v = 0; v < kMaxVoices; ++v) {
        SamplerVoice& voice = m_voices[v];
        if (!voice.active || !voice.currentZone) continue;

        const SamplerZone* zone = voice.currentZone;
        auto* mmap = zone->mmapFile.get();
        
        // Direct pointer access for performance
        const float* dataL = mmap ? reinterpret_cast<const float*>(mmap->getData()) : zone->left.data();
        const float* dataR = (mmap && mmap->getNumChannels() > 1) ? dataL + mmap->getNumSamples() : (zone->right.size() == zone->left.size() ? zone->right.data() : dataL);
        uint64_t maxIdx = mmap ? mmap->getNumSamples() : zone->left.size();

        // TPT SVF Coeffs (Computed once per block)
        float g = std::tan(M_PI * voice.filterCutoff / m_sampleRate);
        float k = 2.0f - 2.0f * voice.filterResonance; // Q mapping
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        for (size_t i = 0; i < numFrames; ++i) {
            if (i % 32 == 0) voice.updateSlide(); // Control rate update
            
            float envGain = voice.envelope.getNext() * voice.velocity;
            if (voice.envelope.getState() == ADSR_OFF) { voice.markAsAvailable(); break; }

            double pos = voice.playbackPos;
            size_t idx = static_cast<size_t>(pos);

            if (idx >= 1 && idx < maxIdx - 2) {
                float frac = static_cast<float>(pos - idx);
                
                // --- 4-POINT CUBIC HERMITE ---
                auto interp = [&](const float* data) {
                    float s0 = data[idx - 1], s1 = data[idx], s2 = data[idx + 1], s3 = data[idx + 2];
                    return s1 + 0.5f * frac * (s2 - s0 + frac * (2.0f * s0 - 5.0f * s1 + 4.0f * s2 - s3 + frac * (3.0f * (s1 - s2) + s3 - s0)));
                };

                float sL = interp(dataL) * envGain;
                float sR = interp(dataR) * envGain;

                // --- TPT SVF FILTER (Stereo) ---
                auto processFilter = [&](float in, float& s1, float& s2) {
                    float v3 = in - s2;
                    float v1 = a1 * s1 + a2 * v3;
                    float v2 = s2 + a2 * s1 + a3 * v3;
                    s1 = 2.0f * v1 - s1;
                    s2 = 2.0f * v2 - s2;
                    return v2; // Low-pass
                };

                outputL[i] += processFilter(sL, voice.filterLZ1, voice.filterLZ2);
                outputR[i] += processFilter(sR, voice.filterRZ1, voice.filterRZ2);

                voice.playbackPos += voice.currentSpeed;

                if (m_isLooping && voice.playbackPos >= m_loopEnd) {
                    voice.playbackPos = m_loopStart + std::fmod(voice.playbackPos - m_loopStart, m_loopEnd - m_loopStart);
                }
            } else {
                voice.markAsAvailable();
                break;
            }
        }
    }
}

} // namespace Aura::Core::DSP::Synthesis

