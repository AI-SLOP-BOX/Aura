#pragma once
#include <string>
#include <optional>
#include <atomic>
#include <cstring>
#include "concurrency/lock_free.hpp"

namespace Aura::Core {

/**
 * @brief StatusQueue: Truly lock-free communication for engine health logs.
 * HONEST FIX: Replaced std::string with fixed-size char array for RT-safety.
 */
class StatusQueue {
public:
    enum class Severity { Info, Warning, Error, Critical };

    struct Message {
        Severity severity;
        char text[128];
    };

    /**
     * @brief Pushes a status update from the Audio thread.
     * HONEST FIX: No heap allocation here.
     */
    void pushFromAudio(Severity severity, const char* text) {
        Message m;
        m.severity = severity;
        std::strncpy(m.text, text, sizeof(m.text) - 1);
        m.text[sizeof(m.text) - 1] = '\0';
        m_queue.push(m);
    }

    /**
     * @brief Pops a status message for the UI thread.
     */
    bool pop(Message& out) {
        auto msg = m_queue.pop();
        if (msg) {
            out = *msg;
            return true;
        }
        return false;
    }

private:
    Concurrency::SPSCQueue<Message, 256> m_queue;
};

} // namespace Aura::Core
