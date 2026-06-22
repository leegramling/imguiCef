#pragma once

#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_life_span_handler.h"
#include <chrono>
#include <mutex>
#include <vector>

class CefRenderHandlerImpl : public CefRenderHandler {
public:
    CefRenderHandlerImpl(int width, int height);
    
    // CefRenderHandler methods
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                        PaintElementType type,
                        const RectList& dirtyRects,
                        const void* buffer,
                        int width, int height) override;
    
    // Custom methods
    void GetTextureData(std::vector<uint8_t>& data, int& width, int& height);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
    double GetPaintFps() const;
    void Resize(int width, int height);
    
private:
    mutable std::mutex m_Mutex;
    std::vector<uint8_t> m_Buffer;
    int m_Width;
    int m_Height;
    bool m_IsDirty;
    double m_PaintFps;
    int m_PaintSamples;
    std::chrono::steady_clock::time_point m_LastPaintSample;
    
    IMPLEMENT_REFCOUNTING(CefRenderHandlerImpl);
};

class CefClientImpl : public CefClient, public CefLifeSpanHandler {
public:
    CefClientImpl(CefRefPtr<CefRenderHandlerImpl> renderHandler);
    
    // CefClient methods
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return m_RenderHandler;
    }
    
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    
    // CefLifeSpanHandler methods
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    
    // Custom methods
    CefRefPtr<CefBrowser> GetBrowser() const { return m_Browser; }
    
private:
    CefRefPtr<CefRenderHandlerImpl> m_RenderHandler;
    CefRefPtr<CefBrowser> m_Browser;
    
    IMPLEMENT_REFCOUNTING(CefClientImpl);
};
