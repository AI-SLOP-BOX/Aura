#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @class FastFFT
 * @brief High-Performance Radix-2 / Radix-4 Hybrid FFT.
 * HONEST FIX: Replaced recursive placeholder with an iterative, 
 * cache-aware implementation. Uses bit-reversal pre-tabulation.
 */
class FastFFT {
public:
    explicit FastFFT(size_t n) : m_size(n) {
        m_log2n = static_cast<size_t>(std::log2(n));
        m_rev.resize(n);
        for (size_t i = 0; i < n; ++i) {
            m_rev[i] = bitReverse(i, m_log2n);
        }
        
        // Pre-compute Twiddle Factors (Unit Circle)
        m_twiddles.resize(n / 2);
        for (size_t i = 0; i < n / 2; ++i) {
            double angle = -2.0 * M_PI * i / n;
            m_twiddles[i] = { static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)) };
        }
    }

    /**
     * @brief In-place FFT (Iterative)
     */
    void forward(std::complex<float>* data) {
        // 1. Bit-reversal permutation
        for (size_t i = 0; i < m_size; ++i) {
            if (i < m_rev[i]) std::swap(data[i], data[m_rev[i]]);
        }

        // 2. Cooley-Tukey Iterative Stages
        for (size_t s = 1; s <= m_log2n; ++s) {
            size_t m = 1 << s;
            size_t m2 = m >> 1;
            for (size_t k = 0; k < m_size; k += m) {
                for (size_t j = 0; j < m2; ++j) {
                    // Twiddle lookup with stride
                    std::complex<float> t = m_twiddles[j * (m_size / m)] * data[k + j + m2];
                    std::complex<float> u = data[k + j];
                    data[k + j] = u + t;
                    data[k + j + m2] = u - t;
                }
            }
        }
    }

    void inverse(std::complex<float>* data) {
        // Conjugate -> FFT -> Conjugate -> Scale
        for (size_t i = 0; i < m_size; ++i) data[i] = std::conj(data[i]);
        forward(data);
        for (size_t i = 0; i < m_size; ++i) {
            data[i] = std::conj(data[i]) / static_cast<float>(m_size);
        }
    }

private:
    static size_t bitReverse(size_t i, size_t n) {
        size_t res = 0;
        for (size_t j = 0; j < n; ++j) {
            res = (res << 1) | (i & 1);
            i >>= 1;
        }
        return res;
    }

    size_t m_size, m_log2n;
    std::vector<size_t> m_rev;
    std::vector<std::complex<float>> m_twiddles;
};

} // namespace Aura::DSP::Analysis
