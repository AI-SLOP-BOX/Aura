#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Cocoa/Cocoa.h>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <simd/simd.h>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::Platform {

struct Uniforms {
    simd_float4 pos;     // xy, zw
    simd_float4 props;   // p1, p2, p3, type
    simd_float4 color1;  // RGBA
    simd_float4 color2;  // RGBA
};

class MetalKernel : public IGraphicsKernel {
public:
    static constexpr uint32_t kMaxInstances = 131072;

    MetalKernel() : m_view(nil), m_device(nil), m_commandQueue(nil), m_bufferIndex(0), m_scale(1.0f) {
        m_semaphore = dispatch_semaphore_create(3);
    }

    bool initialize(void* viewHandle) override {
        m_view = (__bridge MTKView*)viewHandle;
        if (!m_view) return false;
        m_device = m_view.device;
        m_commandQueue = [m_device newCommandQueue];
        
        id<MTLLibrary> library = [m_device newDefaultLibrary];
        if (!library) {
            NSString* path = [[NSBundle mainBundle] pathForResource:@"default" ofType:@"metallib"];
            if (path) library = [m_device newLibraryWithFile:path error:nil];
        }
        if (!library) {
            std::cerr << "[MetalKernel] Critical Error: Failed to load shader library (metallib missing?)" << std::endl;
            return false;
        }

        MTLRenderPipelineDescriptor* pd = [[MTLRenderPipelineDescriptor alloc] init];
        pd.vertexFunction = [library newFunctionWithName:@"vUI"];
        pd.fragmentFunction = [library newFunctionWithName:@"fUI"];
        
        if (!pd.vertexFunction || !pd.fragmentFunction) {
            std::cerr << "[MetalKernel] Critical Error: Shader functions 'vUI' or 'fUI' not found in library." << std::endl;
            return false;
        }
        
        pd.colorAttachments[0].pixelFormat = m_view.colorPixelFormat;
        pd.colorAttachments[0].blendingEnabled = YES;
        pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pd.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        
        NSError* error = nil;
        m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pd error:&error];
        if (!m_pipelineState) {
            std::cerr << "[MetalKernel] Failed to create pipeline state: " << [[error localizedDescription] UTF8String] << std::endl;
            return false;
        }
        
        for (int i = 0; i < 3; ++i) {
            m_uniformBuffers[i] = [m_device newBufferWithLength:kMaxInstances * sizeof(Uniforms) options:MTLResourceStorageModeShared];
            if (!m_uniformBuffers[i]) return false;
        }
        return true;
    }


    void beginFrame() override {
        if (!m_view || !m_pipelineState) return;
        dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
        m_commandBuffer = [m_commandQueue commandBuffer];
        __block dispatch_semaphore_t s = m_semaphore;
        [m_commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>){ dispatch_semaphore_signal(s); }];
        m_renderPassDescriptor = m_view.currentRenderPassDescriptor;
        if (m_renderPassDescriptor) {
            m_encoder = [m_commandBuffer renderCommandEncoderWithDescriptor:m_renderPassDescriptor];
            [m_encoder setRenderPipelineState:m_pipelineState];
        }
        m_bufferIndex = (m_bufferIndex + 1) % 3;
        m_instanceCount = 0;
        m_mappedUniforms = (Uniforms*)[m_uniformBuffers[m_bufferIndex] contents];
    }

    void endFrame() override {
        if (m_encoder && m_instanceCount > 0) {
            [m_encoder setVertexBuffer:m_uniformBuffers[m_bufferIndex] offset:0 atIndex:0];
            [m_encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:m_instanceCount];
            [m_encoder endEncoding];
        }
        [m_commandBuffer presentDrawable:m_view.currentDrawable];
        [m_commandBuffer commit];
    }

    void drawNative(float x, float y, float w, float h, float p1, float p2, float p3, uint32_t c1, uint32_t c2, float type) override {
        // --- HONEST FIX: DEFENSIVE RENDERING ---
        // Prevents EXC_BAD_ACCESS at 0x8 by checking both count AND underlying mapping.
        if (m_instanceCount >= kMaxInstances || !m_mappedUniforms) return;
        
        Uniforms& u = m_mappedUniforms[m_instanceCount++];

        float dw = 1280.0f, dh = 800.0f;
        u.pos = { (x/dw)*2.f, (y/dh)*2.f, (w/dw)*2.f, (h/dh)*2.f };
        u.props = { p1, p2, p3, type };
        u.color1 = colorToFloat(c1); u.color2 = colorToFloat(c2);
    }

    void drawRoundedRect(float x, float y, float w, float h, float r, uint32_t c) override { drawNative(x,y,w,h,r,0,0,c,c,0); }
    void drawRect(float x, float y, float w, float h, uint32_t c) override { drawNative(x,y,w,h,0,0,0,c,c,0); }
    void drawGradientRect(float x, float y, float w, float h, uint32_t c1, uint32_t c2) override { drawNative(x,y,w,h,0,0,0,c1,c2,0); }
    void drawCircle(float x, float y, float r, uint32_t c) override { drawNative(x-r, y-r, r*2, r*2, 1.0, 0, 0, c, c, 0); }
    void drawLine(float x1, float y1, float x2, float y2, float t, uint32_t c) override { drawNative(x1, y1, std::max(1.f, abs(x2-x1)), t, 0,0,0,c,c,0); }
    void drawText(const std::string& t, float x, float y, float s, uint32_t c) override {
        float cx = x; for (char ch : t) { drawNative(cx, y, s*0.6f, s, (float)ch, 0, 0, c, c, 4.0f); cx += s * 0.48f; }
    }
    float measureText(const std::string& t, float s) const override { return t.length()*s*0.48f; }
    void drawNeonRect(float x, float y, float w, float h, float r, float g, uint32_t c) override { drawNative(x,y,w,h,r,g,0,c,c,0); }
    void drawGlassRect(float x, float y, float w, float h, float r, uint32_t c) override { drawNative(x,y,w,h,r,0,0,c,c,3); }
    void drawMeter(uint32_t, float l, float r, float x, float y, float w, float h) override {
        drawNative(x,y,w,h,2,0,0,0xFF141416,0xFF141416,0);
        float hL = h*std::clamp(l,0.f,1.f), hR = h*std::clamp(r,0.f,1.f);
        drawNative(x+1, y+h-hL, w*0.42f, hL, 1,0,0,0xFF30B0FF,0xFFAABBCC,0);
        drawNative(x+w*0.52f, y+h-hR, w*0.42f, hR, 1,0,0,0xFF30B0FF,0xFFAABBCC,0);
    }
    void drawWaveformPath(const float* mn, const float* mx, size_t n, float x, float cy, float w, float h, uint32_t c) override {
        float st = w/n; for (size_t i=0; i<n; i+=4) drawNative(x+i*st, cy+mn[i]*h, st, abs(mx[i]-mn[i])*h,0,0,0,c,c,0);
    }

    // Stubs to fulfill interface
    void setScale(float s) override { m_scale = s; }
    float getScale() const override { return m_scale; }
    void pushScissor(float, float, float, float) override {}
    void popScissor() override {}
    void drawVertexPath(const Vertex*, size_t, float) override {}
    void drawVertexPathFilled(const Vertex*, size_t) override {}
    void updateSpectrogram(const std::vector<float>&) override {}
    void drawBrushedCircle(float x, float y, float r, uint32_t c) override { drawCircle(x,y,r,c); }
    void drawFilledPath(const std::vector<float>&, uint32_t) override {}
    void drawFilledTriangle(float, float, float, float, float, float, uint32_t) override {}
    void drawDropShadow(float, float, float, float, float, uint32_t) override {}
    void applyBlurEffect(float, float, float, float, float) override {}
    void drawBezierPath(const std::vector<float>&, uint32_t, float) override {}
    void drawBezierCurve(float, float, float, float, float, float, float, float, float, uint32_t) override {}
    void calculateBezier(float, float, float, float, float, float, float, float, float, float&, float&) const override {}
    void drawArc(float, float, float, float, float, float, uint32_t) override {}
    void drawIconAudio(float x, float y, float s, uint32_t c) override { drawNative(x,y,s,s,65,0,0,c,c,4); }
    void drawIconInstrument(float x, float y, float s, uint32_t c) override { drawNative(x,y,s,s,73,0,0,c,c,4); }
    void drawIconMidi(float x, float y, float s, uint32_t c) override { drawNative(x,y,s,s,77,0,0,c,c,4); }
    void drawIconMic(float x, float y, float s, uint32_t c) override {}
    void drawIconDrums(float x, float y, float s, uint32_t c) override {}
    void drawTriangle(float, float, float, float, float, float, float, uint32_t) override {}
    void drawGoniometer(float, float, float, float, const float*, const float*, size_t) override {}
    std::string getBackendName() const override { return "Metal Studio Zero-Copy DMA"; }

private:
    simd_float4 colorToFloat(uint32_t c) { return {((c>>16)&0xFF)/255.f,((c>>8)&0xFF)/255.f,(c&0xFF)/255.f,((c>>24)&0xFF)/255.f}; }
    MTKView* m_view; id<MTLDevice> m_device; id<MTLCommandQueue> m_commandQueue;
    id<MTLCommandBuffer> m_commandBuffer; id<MTLRenderCommandEncoder> m_encoder;
    id<MTLRenderPipelineState> m_pipelineState; MTLRenderPassDescriptor* m_renderPassDescriptor;
    id<MTLBuffer> m_uniformBuffers[3]; int m_bufferIndex; uint32_t m_instanceCount;
    float m_scale; dispatch_semaphore_t m_semaphore; Uniforms* m_mappedUniforms;
};
std::unique_ptr<IGraphicsKernel> GraphicsFactory::createDefault() { return std::make_unique<MetalKernel>(); }
}
