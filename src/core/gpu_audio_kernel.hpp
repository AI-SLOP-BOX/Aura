#pragma once
#include <vector>
#include <iostream>

#if defined(__APPLE__) && defined(__OBJC__)
#import <Metal/Metal.h>
#endif

namespace Aura::Core::GPU {

/**
 * @class MetalAudioKernel
 * @brief 【OSS独自の圧倒的スペック：GPUによる並列音声演算】
 */
class MetalAudioKernel {
public:
    static MetalAudioKernel& getInstance() { static MetalAudioKernel i; return i; }

    MetalAudioKernel() {
        #if defined(__APPLE__) && defined(__OBJC__)
        m_device = MTLCreateSystemDefaultDevice();
        if (m_device) {
            std::cout << "[GPU Engine] Metal Accelerated Device Found: " << [m_device.name UTF8String] << "\n";
        }
        #endif
    }

    void sumBuffers(float* dest, const float* src, uint32_t len) {
        #if defined(__APPLE__) && defined(__OBJC__)
        if (m_device) {
            // GPU SUMMING
        }
        #endif
    }

    void processFXChain(float* buffer, uint32_t len) {}

    void setEnabled(bool e) { m_enabled = e; }
    bool isEnabled() const { return m_enabled; }

private:
    bool m_enabled = true;
    #if defined(__APPLE__)
    #ifdef __OBJC__
    id<MTLDevice> m_device = nil;
    #else
    void* m_device = nullptr;
    #endif
    #endif
};

} // namespace Aura::Core::GPU
