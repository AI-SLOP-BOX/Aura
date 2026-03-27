#include "ivoice.hpp"

namespace Aura::DSP::Synthesis {

/**
 * @class VoiceManager
 * @brief Global coordinator for polyphony and resource management.
 */
class VoiceManager {
public:
    static constexpr size_t kMaxGlobalVoices = 32;

    VoiceManager() = default;

    /**
     * @brief Triggers a voice from the pool with zero allocation.
     */
    void triggerVoice(uint8_t note, uint8_t velocity) {
        // 1. Check if note is already playing
        for (auto& v : m_voices) {
            if (v->isActive() && v->getNote() == note) {
                v->trigger(static_cast<float>(note), static_cast<float>(velocity) / 127.0f);
                return;
            }
        }

        // 2. Find free voice
        for (auto& v : m_voices) {
            if (!v->isActive()) {
                v->trigger(static_cast<float>(note), static_cast<float>(velocity) / 127.0f);
                return;
            }
        }

        // 3. Voice Stealing (Simplistic: steal the first one)
        if (!m_voices.empty()) {
            m_voices[0]->trigger(static_cast<float>(note), static_cast<float>(velocity) / 127.0f);
        }
    }

    void render(float* l, float* r, size_t numFrames) {
        for (auto& v : m_voices) {
            if (v->isActive()) v->process(l, r, numFrames); 
        }
    }

    void releaseVoice(uint8_t note) {
        for (auto& v : m_voices) {
            if (v->isActive() && v->getNote() == note) v->release();
        }
    }

    void addVoice(std::unique_ptr<Core::DSP::Synthesis::IVoice> voice) {
        m_voices.push_back(std::move(voice));
    }

private:
    std::vector<std::unique_ptr<Core::DSP::Synthesis::IVoice>> m_voices;
};

} // namespace Aura::DSP::Synthesis
