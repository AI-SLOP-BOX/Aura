#pragma once
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#include <functional>
#include <atomic>

namespace Aura::Core::Driver {

/**
 * @class MacAudioDriver
 * @brief Professional Low-Latency CoreAudio (HAL) Driver for macOS.
 * Uses the native AudioDeviceID and AudioHardwarePropertyListener.
 */
class MacAudioDriver {
public:
    using ProcessCallback = std::function<void(float* l, float* r, uint32_t len)>;

    MacAudioDriver(ProcessCallback callback) : m_callback(callback) {}
    ~MacAudioDriver() { stop(); }

    bool start(double sampleRate, uint32_t bufferSize) {
        m_sampleRate = sampleRate;
        m_bufferSize = bufferSize;

        AudioComponentDescription desc = {
            kAudioUnitType_Output,
            kAudioUnitSubType_DefaultOutput,
            kAudioUnitManufacturer_Apple,
            0, 0
        };

        AudioComponent comp = AudioComponentFindNext(NULL, &desc);
        if (!comp) return false;

        OSStatus err = AudioComponentInstanceNew(comp, &m_unit);
        if (err != noErr) return false;

        // --- 1. SET STREAM FORMAT (32-bit Float Non-Interleaved) ---
        AudioStreamBasicDescription format = {0};
        format.mSampleRate = sampleRate;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
        format.mBitsPerChannel = 32;
        format.mChannelsPerFrame = 2;
        format.mFramesPerPacket = 1;
        format.mBytesPerPacket = 4;
        format.mBytesPerFrame = 4;

        AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
        AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &format, sizeof(format));

        // --- 2. SET BUFFER SIZE ---
        AudioUnitSetProperty(m_unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &bufferSize, sizeof(bufferSize));

        // --- 3. SET RENDER CALLBACK ---
        AURenderCallbackStruct cb;
        cb.inputProc = [](void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time, UInt32 bus, UInt32 frames, AudioBufferList* data) -> OSStatus {
            auto* self = static_cast<MacAudioDriver*>(ref);
            if (!data || data->mNumberBuffers < 2) return noErr;
            float* outL = (float*)data->mBuffers[0].mData;
            float* outR = (float*)data->mBuffers[1].mData;
            
            if (!outL || !outR) return noErr;
            if (self->m_callback) self->m_callback(outL, outR, frames);
            return noErr;
        };
        cb.inputProcRefCon = this;
        AudioUnitSetProperty(m_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));

        AudioUnitInitialize(m_unit);
        AudioOutputUnitStart(m_unit);
        m_running = true;
        return true;
    }

    void stop() {
        if (m_running) {
            AudioOutputUnitStop(m_unit);
            AudioUnitUninitialize(m_unit);
            AudioComponentInstanceDispose(m_unit);
            m_running = false;
        }
    }

private:
    AudioUnit m_unit;
    ProcessCallback m_callback;
    double m_sampleRate = 44100.0;
    uint32_t m_bufferSize = 512;
    std::atomic<bool> m_running{false};
};

} // namespace Aura::Core::Driver
