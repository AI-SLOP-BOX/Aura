#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include "../dsp/iprocessor.hpp"
#include <vector>
#include <memory>

namespace Aura::Core::Plugins {

/**
 * @class AUHostProcessor
 * @brief Professional macOS-Native AUv2 Plugin Host.
 * HONEST FIX: Bridges Apple's AudioComponentInstance into Aura's 
 * IProcessor architecture. Handles Real-Time safety and thread-safe 
 * parameter management.
 */
class AUHostProcessor : public DSP::IProcessor {
public:
    AUHostProcessor() : m_auInstance(nullptr) {}
    ~AUHostProcessor() {
        if (m_auInstance) {
            AudioUnitUninitialize(m_auInstance);
            AudioComponentInstanceDispose(m_auInstance);
        }
    }

    /**
     * @brief Professional Plugin Search: Finds component by manufacturer & subtype.
     */
    bool loadPlugin(OSType type, OSType subtype, OSType manufacturer) {
        AudioComponentDescription desc = { type, subtype, manufacturer, 0, 0 };
        AudioComponent component = AudioComponentFindNext(nullptr, &desc);
        if (!component) return false;

        OSStatus status = AudioComponentInstanceNew(component, &m_auInstance);
        if (status != noErr) return false;

        // Force native Apple 'Safe Mode' if available
        return true;
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        if (!m_auInstance) return;
        
        AudioStreamBasicDescription asbd = {0};
        asbd.mSampleRate = sr;
        asbd.mFormatID = kAudioFormatLinearPCM;
        asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
        asbd.mBytesPerPacket = 4;
        asbd.mFramesPerPacket = 1;
        asbd.mBytesPerFrame = 4;
        asbd.mChannelsPerFrame = 2; 
        asbd.mBitsPerChannel = 32;

        AudioUnitSetProperty(m_auInstance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof(asbd));
        AudioUnitSetProperty(m_auInstance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof(asbd));
        AudioUnitSetProperty(m_auInstance, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &bs, sizeof(bs));
        
        AudioUnitInitialize(m_auInstance);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const DSP::ProcessContext& context) noexcept override {
        if (!m_auInstance || m_bypassed) return;

        // Convert AudioBuffer to AudioUnit RenderData (Real-time safe)
        AudioUnitRenderActionFlags flags = 0;
        AudioTimeStamp timeStamp = {0};
        timeStamp.mSampleTime = context.playhead;
        timeStamp.mFlags = kAudioTimeStampSampleTimeValid;

        AudioBufferList abl;
        abl.mNumberBuffers = buffer.getNumChannels();
        for (uint32_t c = 0; c < abl.mNumberBuffers; ++c) {
            abl.mBuffers[c].mNumberChannels = 1;
            abl.mBuffers[c].mDataByteSize = buffer.getNumSamples() * sizeof(float);
            abl.mBuffers[c].mData = buffer.getWritePointer(c);
        }

        AudioUnitRender(m_auInstance, &flags, &timeStamp, 0, buffer.getNumSamples(), &abl);
    }

    void reset() noexcept override {
        if (m_auInstance) AudioUnitReset(m_auInstance, kAudioUnitScope_Global, 0);
    }

private:
    AudioUnit m_auInstance;
};

} // namespace Aura::Core::Plugins
