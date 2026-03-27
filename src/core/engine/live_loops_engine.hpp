#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "../../dsp/iprocessor.hpp"
#include "tempo_map.hpp"

namespace Aura::Core::Engine {

struct Clip {
    uint32_t trackId;
    std::string filePath;
    double lengthBeats;
    bool loop = true;
};

/**
 * @class LiveLoopsEngine
 * @brief Logic Pro-style Cell triggering with professional quantization.
 * Synchronizes clip launching to the next bar boundary using TempoMap.
 */
class LiveLoopsEngine {
public:
    static constexpr uint32_t kRows = 8;
    static constexpr uint32_t kCols = 8;

    struct CellState {
        std::atomic<bool> queued{false};
        std::atomic<bool> playing{false};
        std::atomic<float> progress{0.0f};
        std::shared_ptr<Clip> clip;
    };

    static LiveLoopsEngine& getInstance() { static LiveLoopsEngine i; return i; }

    void triggerCell(uint32_t row, uint32_t col) {
        if (row < kRows && col < kCols) {
            m_cells[row][col].queued.store(true, std::memory_order_release);
        }
    }

    void stopCell(uint32_t row, uint32_t col) {
        if (row < kRows && col < kCols) {
            m_cells[row][col].queued.store(false, std::memory_order_release);
        }
    }

    /**
     * @brief Process loop logic on RT thread.
     * Manages bar-sync launching.
     */
    void update(uint64_t playhead, double sr) {
        auto& tm = TempoMap::getInstance();
        double beats = tm.samplesToBeats(playhead, sr);
        
        // Simple 4-beat bar quantization logic
        bool isBarStart = std::abs(std::fmod(beats, 4.0)) < 0.01;

        if (isBarStart && !m_lastWasBarStart) {
            for (uint32_t r = 0; r < kRows; ++r) {
                for (uint32_t c = 0; c < kCols; ++c) {
                    bool q = m_cells[r][c].queued.load(std::memory_order_acquire);
                    m_cells[r][c].playing.store(q, std::memory_order_release);
                }
            }
        }
        m_lastWasBarStart = isBarStart;

        // Update progress for UI
        for (uint32_t r = 0; r < kRows; ++r) {
            for (uint32_t c = 0; c < kCols; ++c) {
                if (m_cells[r][c].playing.load()) {
                    auto& cell = m_cells[r][c];
                    if (cell.clip) {
                        double current = std::fmod(beats, cell.clip->lengthBeats);
                        cell.progress.store(static_cast<float>(current / cell.clip->lengthBeats));
                    }
                } else {
                    m_cells[r][c].progress.store(0.0f);
                }
            }
        }
    }

    bool isCellPlaying(uint32_t r, uint32_t c) const { return m_cells[r][c].playing.load(); }
    float getCellProgress(uint32_t r, uint32_t c) const { return m_cells[r][c].progress.load(); }

private:
    LiveLoopsEngine() {
        for (auto& row : m_cells) {
            for (auto& cell : row) {
                cell.queued = false;
                cell.playing = false;
                cell.progress = 0.0f;
            }
        }
    }

    CellState m_cells[kRows][kCols];
    bool m_lastWasBarStart = false;
};

} // namespace Aura::Core::Engine
