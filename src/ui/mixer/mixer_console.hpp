#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "../../graphics/graphics_kernel.hpp"
#include "../../core/engine/track.hpp"
#include "channel_strip.hpp"

namespace Aura::UI::Mixer {

class MixerConsole {
public:
    MixerConsole() {}

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, 
                const std::vector<std::shared_ptr<::Aura::Core::Engine::Track>>& tracks) {
        
        kernel.drawGradientRect(x, y, w, h, 0xFF141416, 0xFF0A0A0C);
        kernel.drawLine(x, y, x + w, y, 1.0f, 0xFF333333);
        
        float stripW = 120.0f;
        for (size_t i = 0; i < std::min(tracks.size(), (size_t)16); ++i) {
             renderStrip(kernel, x + (i * stripW), y, stripW, h, *tracks[i]);
        }
    }

    void renderStrip(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const Core::Engine::Track& track) {
        m_strip.render(kernel, x, y, w, h, track);
    }

private:
    ChannelStrip m_strip;
};

} // namespace Aura::UI::Mixer
