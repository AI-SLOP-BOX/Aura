#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "ui_view.hpp"
#include "../../AuraUltimate.hpp"
#include "lcd_display.hpp"
#include "../../ui/mixer/mixer_console.hpp"
#include "arrangement_view.hpp"
#include "floating_plugin_window.hpp"
#include "aura_pro_icons.hpp"
#include "aura_pro_inspector.hpp"
#include "scae_assistant_pane.hpp"
#include "piano_roll_view.hpp"
#include "step_sequencer_view.hpp"
#include "knob_view.hpp"

namespace Aura::Graphics::UI {

/**
 * @class AuraProUI
 * @brief Logic Pro 11 Professional Master Workspace.
 */
class AuraProUI {
public:
    enum class EditorMode { PianoRoll, StepSeq, SmartControls };

    AuraProUI() : m_width(1280), m_height(800) {
        m_pluginWindows.push_back({420, 200, 480, 320, "Master Compressor", true});
        m_loopLibrary = {"808 Kit", "Warm Piano", "Crystal Pad", "Deep Synth", "Logic Pulse", "Aura Strings", "Studio Bass"};
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float w, float h, ::Aura::AuraEngine& engine) {
        m_width = w; m_height = h; 
        m_controlBarH = 54.0f;
        m_sidebarW = 280.0f;
        m_libraryW = 280.0f;
        m_editorH = (h * 0.44f);

        auto& coreEng = Aura::Core::Engine::AuraUnifiedEngine::getInstance();
        const auto& tracks = coreEng.getTracks();

        kernel.drawRect(0, 0, w, h, 0xFF0A0A0C); // Logic 'Space' Deep Gray

        // --- 1. GLOBAL TRACK ARRANGEMENT ---
        float sidebarsW = (m_libraryVisible ? m_libraryW : 0) + (m_inspectorVisible ? m_sidebarW : 0);
        float mainX = sidebarsW;
        float mainW = w - sidebarsW;
        float mainH = h - m_controlBarH - (m_editorVisible ? m_editorH : 0);
        m_arrangement.render(kernel, mainX, m_controlBarH, mainW, mainH, tracks, m_scrollX);

        // --- 2. THE PINNED SIDEBARS ---
        if (m_libraryVisible) {
            kernel.drawGlassRect(0, m_controlBarH, m_libraryW, h - m_controlBarH, 2, 0xFF141416);
            renderLibrary(kernel, 10, m_controlBarH + 20, m_libraryW - 20, h - m_controlBarH - 40);
        }

        if (m_inspectorVisible) {
            float ix = m_libraryVisible ? m_libraryW : 0;
            kernel.drawRect(ix, m_controlBarH, m_sidebarW, h - m_controlBarH, 0xFF1C1C1E);
            if (!tracks.empty()) {
                m_professionalInspector.render(kernel, ix + 10, m_controlBarH + 10, m_sidebarW - 20, 280, *tracks[0]);
                m_mixer.renderStrip(kernel, ix + 10, h - 340, m_sidebarW - 20, 330, *tracks[0]);
            }
            kernel.drawLine(ix + m_sidebarW - 1, m_controlBarH, ix + m_sidebarW - 1, h, 1.2f, 0xFF000000);
        }

        // --- 3. EDITOR / PIANO ROLL ---
        if (m_editorVisible) {
            float ey = h - m_editorH;
            kernel.drawRect(mainX, ey, mainW, m_editorH, 0xFF141416);
            kernel.drawLine(mainX, ey, w, ey, 2.0f, 0xFF3B9EFF); // Studio Blue Highlight
            if (m_editorMode == EditorMode::StepSeq) m_stepSeq.render(kernel, mainX + 24, ey + 24, mainW - 48, m_editorH - 48);
            else { m_pianoRoll.setBounds({mainX, ey, mainW, m_editorH}); m_pianoRoll.render(kernel); }
        }

        // --- 4. TOP CONTROL BAR (Float / Depth) ---
        renderControlBar(kernel, w, m_controlBarH, engine);

        // --- 5. OVERLAYS ---
        if (m_mixerPanelVisible) {
            float my = h * 0.35f, mh = h * 0.65f;
            kernel.drawGlassRect(0, my, w, mh, 0, 0xFF1C1C1E);
            kernel.drawLine(0, my, w, my, 1.2f, 0xFF3B9EFF);
            m_mixer.render(kernel, 40, my + 40, w - 80, mh - 80, tracks);
        }

        for (const auto& win : m_pluginWindows) m_winRenderer.render(kernel, win);
    }

    void renderLibrary(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float w, float h) {
        k.drawText("LIBRARY", x + 8, y + 8, 13, 0xFFF1F5F9);
        for (int i = 0; i < (int)m_loopLibrary.size(); ++i) {
             k.drawGlassRect(x, y + 42 + i*34, w, 30, 8, 0xFF2A2A2D);
             k.drawText(m_loopLibrary[i], x + 12, y + 62 + i*34, 11, 0xFFCBD5E1);
        }
    }

    void renderControlBar(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float w, float h, ::Aura::AuraEngine& engine) {
        kernel.drawGlassRect(0, 0, w, h, 0, 0xFF1C1C1E);
        kernel.drawLine(0, h-1, w, h-1, 1.0f, 0xFF000000); // 1px Separator

        float gx = 16.0f, gs = 32.0f;
        ProfessionalIcons::drawSolo(kernel, gx, 11, gs, m_libraryVisible);         // Toggle Lib
        ProfessionalIcons::drawPower(kernel, gx + 48, 11, gs, m_inspectorVisible); // Toggle Insp
        ProfessionalIcons::drawMute(kernel, gx + 96, 11, gs, m_editorVisible);    // Toggle Edit
        
        float midX = (w - 600) * 0.5f;
        m_lcd.setBounds({midX, 8, 600, 38}); m_lcd.render(kernel);
        
        float tx = midX - 140;
        bool isPlaying = engine.isPlaying();
        kernel.drawRoundedRect(tx, 12, 74, 30, 4, isPlaying ? 0xFF00C7FF : 0xFF2A2A2E);
        kernel.drawText(isPlaying ? "STOP" : "PLAY", tx + 18, 33, 11, isPlaying ? 0xFFFFFFFF : 0xFF00C7FF);
        
        float mx = w - 210, mw = 120, mh = 26;
        kernel.drawMeter(999, engine.getMasterSuite().getLatestMetrics().peakL, engine.getMasterSuite().getLatestMetrics().peakR, mx, 14, mw, mh);
        ProfessionalIcons::drawLock(kernel, w - 46, 14, 28, m_aiPaneVisible);
    }

    bool handleMouseDown(float x, float y) {
        if (y < m_controlBarH) {
            if (x < 46) { m_libraryVisible = !m_libraryVisible; return true; }
            if (x >= 48 && x < 96) { m_inspectorVisible = !m_inspectorVisible; return true; }
            if (x >= 96 && x < 144) { m_editorVisible = !m_editorVisible; return true; }
            float midX = (m_width - 600) * 0.5f;
            float tx = midX - 140;
            if (x >= tx && x < tx + 74) { ::Aura::AuraEngine::getInstance().togglePlayback(); return true; }
            if (x >= m_width - 46) { m_aiPaneVisible = !m_aiPaneVisible; return true; }
        }
        auto& coreEng = Aura::Core::Engine::AuraUnifiedEngine::getInstance();
        float mainX = (m_libraryVisible ? m_libraryW : 0) + (m_inspectorVisible ? m_sidebarW : 0);
        if (x >= mainX && y >= m_controlBarH) {
             if (m_arrangement.handleMouseDown(x, y, coreEng.getTracks(), m_scrollX)) return true;
        }
        return false;
    }

    void handleMouseDrag(float x, float y, float dx, float dy) {
        auto& coreEng = Aura::Core::Engine::AuraUnifiedEngine::getInstance();
        m_arrangement.handleMouseDrag(x, y, dx, dy, coreEng.getTracks(), m_scrollX);
    }
    void handleMouseUp(float x, float y) { m_arrangement.handleMouseUp(); }

    bool handleKeyDown(uint32_t keyCode, bool cmd, bool shift) {
        auto& engine = ::Aura::AuraEngine::getInstance();
        auto& timeline = engine.getTimeline();
        switch (keyCode) {
            case 49: timeline.setPlaying(!timeline.isPlaying()); return true;
            case 15: timeline.setRecording(!timeline.isRecording()); return true;
            case 36: timeline.setPlayhead(0); return true;
            case 11: if (cmd) { m_libraryVisible = !m_libraryVisible; return true; } break;
            case 34: if (cmd) { m_inspectorVisible = !m_inspectorVisible; return true; } break;
            case 46: m_mixerPanelVisible = !m_mixerPanelVisible; return true;
        }
        return false;
    }

private:
    float m_width, m_height, m_controlBarH, m_sidebarW, m_libraryW, m_aiPaneW, m_editorH;
    bool m_inspectorVisible = true, m_mixerVisible = true, m_editorVisible = true, m_aiPaneVisible = false, m_libraryVisible = true, m_mixerPanelVisible = false;
    float m_scrollX = 0;
    EditorMode m_editorMode = EditorMode::StepSeq;
    std::vector<std::string> m_loopLibrary;
    LCDDisplay m_lcd; ArrangementView m_arrangement;
    ::Aura::UI::Mixer::MixerConsole m_mixer; ProfessionalInspector m_professionalInspector;
    SCAEAssistantPane m_aiAssistant; PianoRollView m_pianoRoll; StepSequencerView m_stepSeq;
    FloatingPluginWindow m_winRenderer; std::vector<FloatingPluginWindow::State> m_pluginWindows;
};

} // namespace Aura::Graphics::UI
