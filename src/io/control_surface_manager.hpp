#pragma once

#include <vector>
#include <map>
#include <string>
#include "../core/engine/param_tree.hpp"

namespace Aura::IO {

/**
 * @brief ControlSurface: Professional Hardware Interaction.
 * Standard protocols for MCU (Mackie Control Universal) and HUI.
 */
class ControlSurfaceManager {
public:
    enum class Protocol { MCU, HUI };

    static ControlSurfaceManager& getInstance() { static ControlSurfaceManager i; return i; }

    /**
     * @brief BANKING: Shifts the 8-fader window of the hardware console.
     */
    void bankShift(int delta) {
        m_currentBankOffset = std::max(0, m_currentBankOffset + (delta * 8));
        updateHardware();
    }

    /**
     * @brief HARDWARE -> DAW: Handles incoming MIDI from physical faders.
     */
    void processMidiIn(uint8_t status, uint8_t data1, uint8_t data2) {
        // Logic for Mackie Protocol (Pitch Bend = Fader, V-Pot = CC)
        if ((status & 0xF0) == 0xE0) { // Pitch Bend (Faders 1-8)
            int faderIdx = status & 0x0F;
            float value = ((data2 << 7) | data1) / 16383.0f;
            uint32_t trackId = m_currentBankOffset + faderIdx;
            
            // Interaction with ParamTree (Volume Param usually offset 0)
            Core::Engine::ParamTree::getInstance().setParam(trackId * 10, value);
        }
    }

    /**
     * @brief DAW -> HARDWARE: Sends feedback to motorized faders.
     */
    void updateHardware() {
        // Send MIDI Out to physical faders to match DAW state
    }

private:
    ControlSurfaceManager() : m_currentBankOffset(0) {}
    int m_currentBankOffset;
};

} // namespace Aura::IO
