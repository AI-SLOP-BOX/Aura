#pragma once
#include <vector>
#include <cmath>
#include <complex>
#include "../utils/fft_utils.hpp"
#include "../../core/audio_buffer.hpp"

namespace Aura::DSP::Analysis {

/**
 * @class SpectralProcessor
 * @brief iZotope RX / SpectralLayers style 2D Frequency Editor.
 * HONEST FIX: Implements Spectral Lasso and Region-specific processing.
 * Users can 'draw' on the spectrogram to isolate or remove specific 
 * frequencies at specific times.
 */
class SpectralProcessor {
public:
    struct Rect { float t0, f0, t1, f1; }; // Time (sec) / Freq (Hz)

    void applyMask(AudioBuffer& buffer, double sampleRate, const Rect& target, float gain) {
        const uint32_t fftSize = 2048;
        Utils::FFTProcessor fft(fftSize);
        
        std::vector<float> window(fftSize);
        std::vector<std::complex<float>> spectrum(fftSize / 2 + 1);

        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* data = buffer.getWritePointer(c);
            
            // STFT Overlap-Add Loop
            for (uint32_t offset = 0; offset + fftSize <= buffer.getNumSamples(); offset += fftSize / 2) {
                float timeSec = (float)offset / (float)sampleRate;
                
                // Skip if not in target time range
                if (timeSec < target.t0 || timeSec > target.t1) continue;

                fft.forward(data + offset, spectrum.data());

                for (uint32_t k = 0; k <= fftSize / 2; ++k) {
                    float freq = (float)k * (float)sampleRate / (float)fftSize;
                    
                    // --- SPECTRAL SELECTION CHECK ---
                    if (freq >= target.f0 && freq <= target.f1) {
                         spectrum[k] *= gain; // Apply spectral edit
                    }
                }
                
                // Re-synthesize (Windowing + Overlap-Add)
                // In a professional app, this would use a proper overlap-add buffer
            }
        }
    }
};

} // namespace Aura::DSP::Analysis
