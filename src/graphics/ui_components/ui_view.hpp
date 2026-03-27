#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @struct Bounds
 * @brief Logical UI coordinates. Kernel handles scaling to physical pixels.
 */
struct Bounds {
    float x, y, w, h;
    
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

/**
 * @class View
 * @brief Base class for all Interactive UI components.
 * HONEST ARCHITECTURE: Separates 'Drawing' from 'Layout' and 'Input'.
 */
class View {
public:
    virtual ~View() = default;

    virtual void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) = 0;
    
    // Event Handlers
    virtual bool onMouseDown(float x, float y) { return false; }
    virtual bool onMouseDoubleClick(float x, float y) { return false; }
    virtual bool onMouseRightClick(float x, float y) { return false; }
    virtual bool onMouseDrag(float x, float y, float dx, float dy) { return false; }
    virtual bool onMouseUp(float x, float y) { return false; }
    virtual bool onMouseEnter() { return false; }
    virtual bool onMouseExit() { return false; }

    void setBounds(Bounds b) { m_bounds = b; }
    const Bounds& getBounds() const { return m_bounds; }
    
    void setVisible(bool v) { m_visible = v; }
    bool isVisible() const { return m_visible; }

protected:
    Bounds m_bounds{0, 0, 0, 0};
    bool m_visible = true;
};

/**
 * @class Container
 * @brief Organizes children with basic relative layout capabilities.
 */
class Container : public View {
public:
    void addChild(std::shared_ptr<View> child) {
        m_children.push_back(child);
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;
        
        // --- HONEST CLIPPING: Never draw outside the container's bounds ---
        kernel.pushScissor(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h);
        
        for (auto& child : m_children) {
            if (child->isVisible()) child->render(kernel);
        }
        
        kernel.popScissor();
    }

    bool onMouseDown(float x, float y) override {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            if ((*it)->isVisible() && (*it)->getBounds().contains(x, y)) {
                if ((*it)->onMouseDown(x, y)) {
                    m_capturedChild = *it;
                    return true;
                }
            }
        }
        return false;
    }

    bool onMouseDoubleClick(float x, float y) override {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            if ((*it)->isVisible() && (*it)->getBounds().contains(x, y)) {
                if ((*it)->onMouseDoubleClick(x, y)) return true;
            }
        }
        return false;
    }

    bool onMouseRightClick(float x, float y) override {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            if ((*it)->isVisible() && (*it)->getBounds().contains(x, y)) {
                if ((*it)->onMouseRightClick(x, y)) return true;
            }
        }
        return false;
    }

    bool onMouseDrag(float x, float y, float dx, float dy) override {
        if (m_capturedChild) {
            return m_capturedChild->onMouseDrag(x, y, dx, dy);
        }
        return false;
    }

    bool onMouseUp(float x, float y) override {
        if (m_capturedChild) {
            bool result = m_capturedChild->onMouseUp(x, y);
            m_capturedChild = nullptr;
            return result;
        }
        return false;
    }

protected:
    std::vector<std::shared_ptr<View>> m_children;
    std::shared_ptr<View> m_capturedChild = nullptr;
};

} // namespace Aura::Graphics::UI
