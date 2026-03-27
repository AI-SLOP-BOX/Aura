#pragma once

#include "graphics/ui_components/waveform_renderer.hpp"
#include "graphics/ui_components/smart_controls_ui.hpp"
#include "graphics/ui_components/piano_roll_renderer.hpp"
#include "graphics/ui_components/aura_pro_ui.hpp"
#include "core/engine/param_tree.hpp"
#include "ui/mixer/channel_strip.hpp"
#include "core/engine/timeline_system.hpp"
#include "ui/main/view_transformer.hpp"
#include "AuraUltimate.hpp"
#include <iostream>

namespace Aura::UI::Main {

struct Theme {
    uint32_t bgMain = 0xFF0A0A0B;
    uint32_t bgTrackOdd = 0xFF141416;
    uint32_t bgTrackEven = 0xFF1A1A1C;
    uint32_t bgHeader = 0xFF1C1C1E;
    uint32_t accentBlue = 0xFF3B9EFF;
    uint32_t regionBlue = 0xFF2B8EDF;
    uint32_t textPrimary = 0xFFE0E0E0;
    uint32_t playheadRed = 0xFFFF3B30;
};

class AuraWorkspace {
public:
    static AuraWorkspace& getInstance() {
        static AuraWorkspace instance;
        return instance;
    }

    void initialize(float w, float h) { m_width = w; m_height = h; }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) {
        // --- THE AURA PRO ULTIMATE UI (METAL OPTIMIZED + LIVE ENGINE) ---
        m_proUI.render(kernel, m_width, m_height, ::Aura::AuraEngine::getInstance());
    }

    // HIT TESTING & INTERACTION
    void handleMouseDown(float x, float y) {
        m_draggedRegion = nullptr;
        m_draggedMidi = nullptr;
        m_clickX = x; m_clickY = y;

        // 1. DELEGATE TO PRO UI (Top Bar, LCD, Floating Windows)
        if (m_proUI.handleMouseDown(x, y)) return;
        
        float controlBarH = 46.0f, rulerH = 32.0f;
        float inspectorW = 240.0f, trackHeaderW = 260.0f;
        float arrangementX = inspectorW + trackHeaderW;
        float arrangementY = controlBarH + rulerH;
        
        auto& engine = ::Aura::AuraEngine::getInstance();
        auto& timeline = engine.getTimeline();
        auto& tracks = timeline.getTracks();

        // 2. TIMELINE / REGION DRAGGING
        if (x >= arrangementX && y >= arrangementY) {
            float trackH = 82.0f;
            for (size_t t = 0; t < tracks.size(); ++t) {
                float ty = arrangementY + (t * trackH);
                if (y >= ty && y < ty + trackH) {
                    for (auto& region : tracks[t]->getAudioRegions()) {
                        auto& vt = ViewTransformer::getInstance();
                        float rx = arrangementX + vt.samplesToPixels(region->getMeta().samplePosition);
                        float rw = vt.samplesToPixels(region->getMeta().sampleLength);

                        if (x >= rx && x < rx + rw) {
                            m_draggedRegion = region.get();
                            m_dragStartSamples = region->getMeta().samplePosition;
                            return;
                        }
                    }
                }
            }
        }
    }

    void handleMouseDrag(float x, float y) {
        float dx = x - m_clickX;
        float dy = y - m_clickY;
        
        // 1. Delegate to Pro UI
        m_proUI.handleMouseDrag(x, y, dx, dy);

        // 2. Handle Timeline Dragging
        auto& vt = ViewTransformer::getInstance();
        if (m_draggedRegion) {
            int64_t dSamples = static_cast<int64_t>(vt.pixelsToSamples(dx));
            int64_t newPos = static_cast<int64_t>(m_dragStartSamples) + dSamples;
            m_draggedRegion->setSamplePosition(static_cast<uint64_t>(std::max(0LL, newPos)));
        }
    }

    void handleMouseUp(float x, float y) {
        m_proUI.handleMouseUp(x, y);
        m_draggedRegion = nullptr;
    }

    bool handleKeyDown(uint32_t key, bool cmd, bool shift) {
        return m_proUI.handleKeyDown(key, cmd, shift);
    }

private:
    ::Aura::Graphics::UI::AuraProUI m_proUI;
    AuraWorkspace() = default;
    float m_width = 1280, m_height = 800;
    float m_trackHeight = 100, m_headerWidth = 250, m_timeScale = 0.05f, m_offsetTop = 60.0f;
    Theme m_theme;
    ::Aura::Core::AudioRegion* m_draggedRegion = nullptr;
    ::Aura::Core::MIDIRegion* m_draggedMidi = nullptr;
    uint64_t m_dragStartSamples = 0;
    float m_clickX = 0, m_clickY = 0;

public:
    /**
     * @brief TOOL: Scissors (Split selected region at current playhead)
     */
    void splitSelectedRegion(uint64_t playheadPos) {
        auto& timeline = ::Aura::AuraEngine::getInstance().getTimeline();
        for (auto& track : timeline.getTracks()) {
            // Check Audio Regions
            auto& aRegions = track->getAudioRegions();
            for (size_t i = 0; i < aRegions.size(); ++i) {
                if (playheadPos > aRegions[i]->getMeta().samplePosition && 
                    playheadPos < (aRegions[i]->getMeta().samplePosition + aRegions[i]->getMeta().sampleLength)) {
                    uint64_t rel = playheadPos - aRegions[i]->getMeta().samplePosition;
                    auto splitOne = aRegions[i]->split(rel);
                    if (splitOne) aRegions.push_back(splitOne);
                    return;
                }
            }
        }
    }
};

} // namespace Aura::UI::Main
