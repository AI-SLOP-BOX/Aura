#pragma once
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::Platform {

/**
 * @class VulkanGraphicsKernel
 * @brief High-performance Vulkan implementation for Windows/Linux.
 * (Skeleton Implementation for Compilation & Cross-platform Strategy)
 */
class VulkanGraphicsKernel : public IGraphicsKernel {
public:
    VulkanGraphicsKernel();
    virtual ~VulkanGraphicsKernel();

    bool initialize(void* nativeWindowHandle) override;
    void beginFrame() override;
    void endFrame() override;

    void setScale(float s) override { m_scale = s; }
    float getScale() const override { return m_scale; }

    void updateSpectrogram(const std::vector<float>& data) override;
    void drawMeter(uint32_t trackId, float levelL, float levelR, float x, float y, float w, float h) override;

    void drawRoundedRect(float x, float y, float w, float h, float radius, uint32_t color) override;
    void drawGradientRect(float x, float y, float w, float h, uint32_t colorTop, uint32_t colorBottom) override;
    void drawText(const std::string& text, float x, float y, float size, uint32_t color) override;
    float measureText(const std::string& text, float size) const override;
    void drawCircle(float x, float y, float radius, uint32_t color) override;
    void drawLine(float x1, float y1, float x2, float y2, float thickness, uint32_t color) override;
    void drawFilledPath(const std::vector<float>& points, uint32_t color) override;
    void drawDropShadow(float x, float y, float w, float h, float radius, uint32_t color) override;
    void applyBlurEffect(float x, float y, float w, float h, float intensity) override;
    void drawBezierPath(const std::vector<float>& points, uint32_t color, float thickness) override;
    void drawArc(float cx, float cy, float radius, float start, float end, float thickness, uint32_t color) override;
    void drawVertexPath(const std::vector<Vertex>& vertices, float thickness) override;

    void setScissorRect(float x, float y, float w, float h) override {}
    void clearScissorRect() override {}
    void drawBrushedCircle(float x, float y, float radius, uint32_t color) override {}
    void drawVertexPathFilled(const std::vector<Vertex>& vertices) override {}
    void drawFilledTriangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color) override {}
    void drawNeonRect(float x, float y, float w, float h, float radius, float glow, uint32_t color) override;
    
    void drawIconAudio(float x, float y, float s, uint32_t col) override {}
    void drawIconInstrument(float x, float y, float s, uint32_t col) override {}
    void drawIconMidi(float x, float y, float s, uint32_t col) override {}
    void drawIconMic(float x, float y, float s, uint32_t col) override {}
    void drawIconDrums(float x, float y, float s, uint32_t col) override {}
    
    void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, float thickness, uint32_t color) override {}
    void drawGoniometer(float x, float y, float w, float h, const float* historyL, const float* historyR, size_t count) override;

    std::string getBackendName() const override { return "Vulkan (Unified Logic Driver)"; }

    struct SDFPushConstants {
        float x, y, w, h;
        float colorL, colorR, colorT, colorB; // Packed RGBA
        float radius;
        float type; // 0=Rect, 1=Arc, 2=MSDF
        float border;
        float glow;
    };

private:
    void cleanup();
    void pushSDF(const SDFPushConstants& constants);
    
    float m_scale = 1.0f;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

} // namespace Aura::Graphics::Platform
