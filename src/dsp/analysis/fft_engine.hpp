#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @brief FFTEngine: The mathematical 'Truth' of the DAW.
 * Implements Cooley-Tukey Radix-2 algorithm for spectral processing.
 * Essential for Reverb, Pitch Shift, and Spectral Repair.
 */
class FFTEngine {
public:
    FFTEngine(uint32_t n) : m_n(n), m_log2n(static_cast<uint32_t>(std::log2(n))) {
        prepareBitReversal();
        prepareTwiddles();
    }

    /**
     * @brief FORWARD FFT: Time-domain -> Frequency-domain.
     */
    void forward(const std::vector<float>& realIn, std::vector<std::complex<float>>& complexOut) {
        for (uint32_t i = 0; i < m_n; ++i) complexOut[i] = { realIn[m_bitRev[i]], 0.0f };
        compute(complexOut, false);
    }

    /**
     * @brief INVERSE FFT: Frequency-domain -> Time-domain.
     */
    void inverse(const std::vector<std::complex<float>>& complexIn, std::vector<float>& realOut) {
        std::vector<std::complex<float>> temp = complexIn;
        compute(temp, true);
        for (uint32_t i = 0; i < m_n; ++i) realOut[i] = temp[i].real() / m_n;
    }

private:
    void compute(std::vector<std::complex<float>>& data, bool inverse) {
        for (uint32_t s = 1; s <= m_log2n; ++s) {
            uint32_t m = 1 << s;
            uint32_t m2 = m >> 1;
            for (uint32_t k = 0; k < m_n; k += m) {
                for (uint32_t j = 0; j < m2; ++j) {
                    auto w = m_twiddles[m2 + j];
                    if (inverse) w = std::conj(w);
                    auto u = data[k + j];
                    auto t = w * data[k + j + m2];
                    data[k + j] = u + t;
                    data[k + j + m2] = u - t;
                }
            }
        }
    }

    void prepareBitReversal() {
        m_bitRev.resize(m_n);
        for (uint32_t i = 0; i < m_n; ++i) {
            uint32_t rev = 0;
            for (uint32_t j = 0; j < m_log2n; ++j) if (i & (1 << j)) rev |= (1 << (m_log2n - 1 - j));
            m_bitRev[i] = rev;
        }
    }

    void prepareTwiddles() {
        m_twiddles.resize(m_n);
        for (uint32_t i = 1; i < m_n; i <<= 1) {
            for (uint32_t j = 0; j < i; ++j) {
                float angle = -M_PI * j / i;
                m_twiddles[i + j] = std::polar(1.0f, angle);
            }
        }
    }

    uint32_t m_n, m_log2n;
    std::vector<uint32_t> m_bitRev;
    std::vector<std::complex<float>> m_twiddles;
};

} // namespace Aura::DSP::Analysis
