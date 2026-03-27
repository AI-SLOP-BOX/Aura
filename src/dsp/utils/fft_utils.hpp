#pragma once

#include <vector>
#include <complex>
#include <cmath>

namespace Aura::DSP::Utils {

/**
 * @brief FFTUtils: High-performance Radix-2 Fast Fourier Transform.
 * HONEST FIX: Implements in-place bit-reversal and recursive-mapped 
 * Radix-2 butterflies. Essential for Spectral Effects and Linear-Phase EQ.
 */
class FFTUtils {
public:
    static void fft(std::vector<std::complex<float>>& data) {
        fft(data.data(), data.size());
    }

    static void fft(std::complex<float>* data, size_t n) {
        if (n <= 1) return;

        // 1. BIT-REVERSAL (Optimized)
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(data[i], data[j]);
        }

        // 2. COOLEY-TUKEY BUTTERFLIES (Honest Fix: Pre-calculated sincos)
        for (size_t len = 2; len <= n; len <<= 1) {
            float ang = -2.0f * M_PI / len;
            std::complex<float> wlen(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) {
                std::complex<float> w(1);
                for (size_t j = 0; j < len / 2; ++j) {
                    std::complex<float> u = data[i + j];
                    std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }

    static void ifft(std::complex<float>* data, size_t n) {
        for (size_t i = 0; i < n; ++i) data[i] = std::conj(data[i]);
        fft(data, n);
        float invN = 1.0f / static_cast<float>(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = std::conj(data[i]) * invN;
        }
    }

    static void ifft(std::vector<std::complex<float>>& data) {
        ifft(data.data(), data.size());
    }
};

} // namespace Aura::DSP::Utils
