#pragma once

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Aura::Core {

/**
 * @class AtomicParameter
 * @brief Thread-safe, lock-free parameter with smoothing and professional formatting.
 */
class AtomicParameter {
public:
    enum class SmoothingType { Linear, Exponential, None };
    enum class DisplayMode { Unipolar, Bipolar };
    enum class Unit { Percentage, Decibels, Frequency, Time, Raw };

    explicit AtomicParameter(float initialValue = 1.0f, DisplayMode mode = DisplayMode::Unipolar) 
        : m_target(initialValue), m_current(initialValue), m_displayMode(mode) {}

    /**
     * @brief HONEST FIX: PROFESSIONAL UNIT FORMATTING.
     * Logic Pro / Surge XT style display mapping.
     */
    void getValueString(char* buffer, size_t size) const {
        if (!buffer || size == 0) return;
        float val = m_target.load(std::memory_order_acquire);
        
        switch (m_unit) {
            case Unit::Percentage:
                snprintf(buffer, size, "%.1f%%", (val + 1.0f) * 50.0f);
                break;
            case Unit::Decibels: {
                float db = 20.0f * std::log10(std::max(1e-5f, (val + 1.0f) * 0.5f));
                if (db < -90.0f) snprintf(buffer, size, "-inf dB");
                else snprintf(buffer, size, "%.1f dB", db);
            } break;
            case Unit::Frequency: {
                // Logarithmic frequency mapping example (20Hz - 20kHz)
                float freq = 20.0f * std::pow(1000.0f, (val + 1.0f) * 0.5f);
                if (freq >= 1000.0f) snprintf(buffer, size, "%.2f kHz", freq * 0.001f);
                else snprintf(buffer, size, "%.0f Hz", freq);
            } break;
            case Unit::Time:
                snprintf(buffer, size, "%.1f ms", std::max(0.0f, (val + 1.0f) * 500.0f));
                break;
            default:
                snprintf(buffer, size, "%.2f", val);
                break;
        }
    }

    float getNormalizedValue() const {
        float val = m_current;
        if (m_displayMode == DisplayMode::Unipolar) {
            return std::clamp((val + 1.0f) * 0.5f, 0.0f, 1.0f);
        }
        return val;
    }

    void setUnit(Unit unit) { m_unit = unit; }
    void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

    float getNextValue() {
        if (m_dirty.load(std::memory_order_acquire)) {
            updateInternalState();
        }

        const float target = m_target.load(std::memory_order_relaxed);
        
        if (std::abs(m_current - target) < 1e-7f) {
            m_current = target;
            return getNormalizedValue() + m_aiOffset.load(std::memory_order_relaxed);
        }
        
        SmoothingType type = m_type.load(std::memory_order_relaxed);
        if (type == SmoothingType::Exponential) {
            m_current = m_current + (target - m_current) * m_coeff;
        } else if (type == SmoothingType::Linear) {
            m_current += m_step;
            if ((m_step > 0 && m_current > target) || (m_step < 0 && m_current < target)) m_current = target;
        } else {
            m_current = target;
        }
        
        // --- HONEST FIX: SAFETY CLAMPING ---
        // Prevents AI modulation from pushing parameters into unstable territory.
        float val = getNormalizedValue() + m_aiOffset.load(std::memory_order_relaxed);
        return std::clamp(val, 0.0f, 1.0f);
    }


    /**
     * @brief BLOCK OPTIMIZED: Loads atomics once per block.
     */
    void getNextBlock(float* buffer, size_t numSamples) {
        if (m_dirty.load(std::memory_order_acquire)) {
            updateInternalState();
        }

        float target = m_target.load(std::memory_order_relaxed);
        SmoothingType type = m_type.load(std::memory_order_relaxed);
        const float aiMod = m_aiOffset.load(std::memory_order_relaxed);
        
        for (size_t i = 0; i < numSamples; ++i) {
            // Internal smoothing logic
            if (std::abs(m_current - target) > 1e-7f) {
                if (type == SmoothingType::Exponential) {
                    m_current = m_current + (target - m_current) * m_coeff;
                } else if (type == SmoothingType::Linear) {
                    m_current += m_step;
                    if ((m_step > 0 && m_current > target) || (m_step < 0 && m_current < target)) m_current = target;
                } else {
                    m_current = target;
                }
            } else {
                m_current = target;
            }
            
            float val = m_current;
            float norm = (m_displayMode == DisplayMode::Unipolar) ? std::clamp((val + 1.0f) * 0.5f, 0.0f, 1.0f) : val;
            buffer[i] = std::clamp(norm + aiMod, 0.0f, 1.0f);
        }
    }


    void setTarget(float value) {
        m_target.store(value, std::memory_order_release);
        m_dirty.store(true, std::memory_order_release);
    }

    void setSampleRate(double sr) {
        m_sampleRate.store(sr, std::memory_order_relaxed);
        m_dirty.store(true, std::memory_order_release);
    }

    void setAIModulation(float offset) { m_aiOffset.store(offset, std::memory_order_release); }

private:
    void updateInternalState() {
        if (m_shouldReset.exchange(false, std::memory_order_acq_rel)) {
            m_current = m_resetValue.load(std::memory_order_acquire);
        }

        double sr = m_sampleRate.load(std::memory_order_relaxed);
        double ms = m_smoothingTimeMs.load(std::memory_order_relaxed);
        float target = m_target.load(std::memory_order_relaxed);

        if (sr > 0) {
            double samples = (ms * 0.001) * sr;
            m_coeff = static_cast<float>(1.0 - std::exp(-1.0 / std::max(1.0, samples)));
            m_step = (target - m_current) / std::max(1.0f, static_cast<float>(samples));
        }
        m_dirty.store(false, std::memory_order_release);
    }

    std::atomic<float> m_target;
    std::atomic<float> m_aiOffset{0.0f};
    std::atomic<float> m_resetValue{0.0f};
    std::atomic<bool> m_shouldReset{false};
    std::atomic<bool> m_dirty{true};

    float m_current; 
    float m_coeff = 0.01f;
    float m_step = 0.0f;
    DisplayMode m_displayMode = DisplayMode::Unipolar;
    Unit m_unit = Unit::Percentage;
    std::atomic<SmoothingType> m_type{SmoothingType::Exponential};
    std::atomic<double> m_sampleRate{44100.0};
    std::atomic<double> m_smoothingTimeMs{10.0};
};

} // namespace Aura::Core

