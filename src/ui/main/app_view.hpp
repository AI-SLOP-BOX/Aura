#pragma once

#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <memory>
#include "graphics/graphics_kernel.hpp"
#include "ui/main/workspace.hpp"

namespace Aura::UI::Main {

/**
 * @class AuraAppView: The high-level UI orchestrator.
 */
class AuraAppView {
public:
    static AuraAppView& getInstance() {
        static AuraAppView instance;
        return instance;
    }

    void bootstrap(void* windowHandle, float w, float h, std::function<void(float)> onProgress = nullptr) {
        std::cout << "[UI] Bootstrapping UI..." << std::endl;
        m_running = true;
        m_width = w;
        m_height = h;
        
        m_kernel = Graphics::Platform::GraphicsFactory::createDefault();
        if (m_kernel) {
            std::cout << "[UI] Kernel Created." << std::endl;
            m_kernel->initialize(windowHandle);
        }
        
        AuraWorkspace::getInstance().initialize(w, h);
    }

    void onResize(float w, float h) {
        m_width = w;
        m_height = h;
        AuraWorkspace::getInstance().initialize(w, h);
    }

    void setScale(float s) { if (m_kernel) m_kernel->setScale(s); }

    ::Aura::Graphics::Platform::IGraphicsKernel& getKernel() { return *m_kernel; }

    void updateUI() {
        if (!m_kernel) return;
        
        // --- MANDATORY RECLAMATION ---
        Core::Concurrency::DeferredDeleter::getInstance().performCleanup();
        
        m_kernel->beginFrame();
        AuraWorkspace::getInstance().render(*m_kernel);
        m_kernel->endFrame();
    }

    void handleMouseDown(float x, float y) { AuraWorkspace::getInstance().handleMouseDown(x, y); }
    void handleMouseDrag(float x, float y) { AuraWorkspace::getInstance().handleMouseDrag(x, y); }
    void handleMouseUp(float x, float y) { AuraWorkspace::getInstance().handleMouseUp(x, y); }
    void handleKeyDown(uint32_t key, bool cmd, bool shift) { AuraWorkspace::getInstance().handleKeyDown(key, cmd, shift); }

    bool isRunning() const { return m_running; }

private:
    AuraAppView() = default;
    bool m_running = false;
    float m_width = 1280, m_height = 800;
    std::unique_ptr<::Aura::Graphics::Platform::IGraphicsKernel> m_kernel;
};

} // namespace Aura::UI::Main
