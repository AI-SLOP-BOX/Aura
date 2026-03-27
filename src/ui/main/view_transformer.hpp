#pragma once
#include <cmath>
#include <cstdint>

namespace Aura::UI::Main {

/**
 * @class ViewTransformer
 * @brief Unified Coordinate Mapping for the entire DAW.
 * Ensures hit-testing and rendering always use the same math.
 */
class ViewTransformer {
public:
    static ViewTransformer& getInstance() {
        static ViewTransformer instance;
        return instance;
    }

    void setZoom(float pixelsPerBeat) { m_pixelsPerBeat = pixelsPerBeat; }
    void setBPM(float bpm) { m_bpm = bpm; }
    void setSampleRate(double sr) { m_sampleRate = sr; }

    float beatsToPixels(double beats) const { return static_cast<float>(beats * m_pixelsPerBeat); }
    double pixelsToBeats(float pixels) const { return static_cast<double>(pixels / m_pixelsPerBeat); }

    float samplesToPixels(uint64_t samples) const {
        double beats = (static_cast<double>(samples) / m_sampleRate) * (m_bpm / 60.0);
        return beatsToPixels(beats);
    }

    uint64_t pixelsToSamples(float pixels) const {
        double beats = pixelsToBeats(pixels);
        return static_cast<uint64_t>(beats * (60.0 / m_bpm) * m_sampleRate);
    }

    float getPixelsPerSecond() const {
        return static_cast<float>((m_bpm / 60.0) * m_pixelsPerBeat);
    }

private:
    ViewTransformer() : m_pixelsPerBeat(100.0f), m_bpm(120.0f), m_sampleRate(44100.0) {}
    float m_pixelsPerBeat;
    float m_bpm;
    double m_sampleRate;
};

} // namespace Aura::UI::Main
