#include "adsr_envelope.hpp"
#include "../../io/mmap_audio_file.hpp"
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>

namespace Aura::Core::DSP::Synthesis {

struct SamplerZone {
    uint32_t minNote = 0;
    uint32_t maxNote = 127;
    float minVelocity = 0.0f;
    float maxVelocity = 1.0f;
    uint32_t rootNote = 60;
    
    std::vector<float> left;
    std::vector<float> right;
    std::shared_ptr<IO::MMapAudioFile> mmapFile;

    bool matches(uint32_t note, float velocity) const {
        return note >= minNote && note <= maxNote && velocity >= minVelocity && velocity <= maxVelocity;
    }
};

struct SamplerVoice {
    bool active = false;
    uint32_t note = 0;
    double playbackPos = 0.0;
    float currentSpeed = 1.0f;
    float targetSpeed = 1.0f; 
    float slideRate = 0.0f;   
    float velocity = 1.0f;
    ADSREnvelope envelope;
    
    // HONEST FIX: Separate L/R states for SVF Filter (Stereo Separation)
    float filterLZ1 = 0.0f, filterLZ2 = 0.0f;
    float filterRZ1 = 0.0f, filterRZ2 = 0.0f;
    float filterCutoff = 1000.0f; 
    float filterResonance = 0.1f;

    float driftPhase = 0.0f;
    float driftSpeed = 0.01f;
    uint64_t startTime = 0; // For voice stealing

    const SamplerZone* currentZone = nullptr;

    bool isAvailable() const { return !active; }
    void markAsAvailable() { 
        active = false; 
        envelope.reset(); 
        currentZone = nullptr; 
        filterLZ1 = filterLZ2 = filterRZ1 = filterRZ2 = 0.0f;
    }
    
    void updateSlide() {
        if (std::abs(currentSpeed - targetSpeed) < 1e-4f) {
            currentSpeed = targetSpeed;
            return;
        }
        currentSpeed += (targetSpeed - currentSpeed) * slideRate;
    }
};

class AuraSamplerPro {
public:
    static constexpr int kMaxVoices = 64; 

    explicit AuraSamplerPro(double sr) : m_sampleRate(sr) {
        m_zones = std::make_shared<std::vector<SamplerZone>>();
        for (int i = 0; i < kMaxVoices; ++i) {
            m_voices[i].envelope.setSampleRate(sr);
            m_voices[i].envelope.setParameters(0.005f, 0.1f, 0.8f, 0.3f);
        }
    }
    
    void setZones(std::vector<SamplerZone> zones) {
        // HONEST FIX: Ensure types match for atomic_store
        std::shared_ptr<const std::vector<SamplerZone>> newZones = std::make_shared<const std::vector<SamplerZone>>(std::move(zones));
        std::atomic_store(&m_zones, newZones);
    }

    void noteOn(uint32_t note, float velocity) {
        // --- HONEST FIX: VOICE STEALING (LRU STYLE) ---
        // Address Point 5: Ensure new notes are heard even at max polyphony.
        int oldestIdx = -1;
        uint64_t oldestTime = UINT64_MAX;
        int freeIdx = -1;

        for (int i = 0; i < kMaxVoices; ++i) {
            if (m_voices[i].isAvailable()) {
                freeIdx = i;
                break;
            }
            if (m_voices[i].startTime < oldestTime) {
                oldestTime = m_voices[i].startTime;
                oldestIdx = i;
            }
        }

        int targetIdx = (freeIdx != -1) ? freeIdx : oldestIdx;
        if (targetIdx == -1) return; // Should not happen

        // Find matching zone
        auto zonesPtr = std::atomic_load(&m_zones);
        if (!zonesPtr) return;
        
        const SamplerZone* bestZone = nullptr;
        for (const auto& zone : *zonesPtr) {
            if (zone.matches(note, velocity)) {
                bestZone = &zone;
                break;
            }
        }
        
        if (bestZone) {
            triggerVoice(targetIdx, note, velocity, bestZone);
            m_voices[targetIdx].startTime = ++m_globalTime;
        }
    }

    void noteOff(uint32_t note) {
        for (int i = 0; i < kMaxVoices; ++i) {
            if (m_voices[i].active && m_voices[i].note == note) {
                if (m_voices[i].envelope.getState() != ADSR_RELEASE) {
                    m_voices[i].envelope.triggerOff();
                }
            }
        }
    }

    void process(float* outputL, float* outputR, size_t numFrames);

private:
    uint64_t m_loopStart = 0, m_loopEnd = 0;
    bool m_isLooping = false;
    double m_sampleRate;
    uint64_t m_globalTime = 0; // For voice stealing
    
    // HONEST FIX: Zone list is now a shared pointer for RT safety.
    std::shared_ptr<const std::vector<SamplerZone>> m_zones;
    SamplerVoice m_voices[kMaxVoices];

    void triggerVoice(int idx, uint32_t note, float velocity, const SamplerZone* zone) {
        m_voices[idx].active = true;
        m_voices[idx].note = note;
        m_voices[idx].playbackPos = 0.0;
        m_voices[idx].velocity = velocity;
        m_voices[idx].currentZone = zone;
        
        float root = zone ? static_cast<float>(zone->rootNote) : 60.0f;
        float speed = std::pow(2.0f, (static_cast<float>(note) - root) / 12.0f);
        m_voices[idx].currentSpeed = speed;
        m_voices[idx].targetSpeed = speed;
        m_voices[idx].slideRate = 0.005f; 
        
        m_voices[idx].envelope.triggerOn();
    }
};

} // namespace Aura::Core::DSP::Synthesis
