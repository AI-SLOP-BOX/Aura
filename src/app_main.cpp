#include <iostream>
#include "AuraUltimate.hpp"
#include "ui/platform/macos_window.mm" // Inclusion for single-compilation if preferred or linker handle

/**
 * @brief Aura DAW Ultimate: Professional Entry Point.
 * Initializes the high-performance audio engine and native macOS window.
 */
int main(int argc, char* argv[]) {
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "  Aura DAW Ultimate [2026 Edition] - Booting..." << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    try {
        // 1. ENGINE INITIALIZATION
        ::Aura::Core::Memory::RealtimeMemoryPool::getInstance().initialize(1024 * 1024 * 256); // 256MB
        auto& engine = ::Aura::AuraEngine::getInstance();
        engine.initialize(44100.0, 512);

        // 2. NATIVE WINDOW CREATION (Metal-backed)
        ::Aura::UI::Platform::MacOS_Window window;
        if (!window.initialize(1280, 800, "Aura DAW Ultimate")) {
            throw std::runtime_error("Failed to initialize native window.");
        }

        // 3. MAIN RUN LOOP
        std::cout << "[Runtime] Entering main event loop." << std::endl;
        window.run();

    } catch (const std::exception& e) {
        std::cerr << "[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[Shutdown] Aura DAW Ultimate Closed Safely." << std::endl;
    return 0;
}
