#pragma once
#include <cstdint>

namespace Aura::Core {

/**
 * @struct MusicalTime
 * @brief Unified musical position structure for Engine and UI.
 */
struct MusicalTime {
    int32_t bar;
    int32_t beat;
    int32_t sixteenth;
    int32_t tick;
    double totalBeats;
};

} // namespace Aura::Core
