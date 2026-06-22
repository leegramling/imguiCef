#include "../include/cef_client_impl.h"
#include <cstring>
#include <algorithm>
#include <iostream>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#endif

// CefRenderHandlerImpl implementation
CefRenderHandlerImpl::CefRenderHandlerImpl(int width, int height)
    : m_Width(width),
      m_Height(height),
      m_IsDirty(false),
      m_PaintFps(0.0),
      m_PaintSamples(0),
      m_LastPaintSample(std::chrono::steady_clock::now()) {
    m_Buffer.resize(width * height * 4);
}

void CefRenderHandlerImpl::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    rect = CefRect(0, 0, m_Width, m_Height);
}

void CefRenderHandlerImpl::OnPaint(CefRefPtr<CefBrowser> browser,
                                   PaintElementType type,
                                   const RectList& dirtyRects,
                                   const void* buffer,
                                   int width, int height) {
    ZoneScoped;
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (width != m_Width || height != m_Height) {
        m_Width = width;
        m_Height = height;
        m_Buffer.resize(width * height * 4);
    }
    
    // Copy the entire buffer (BGRA format)
    std::memcpy(m_Buffer.data(), buffer, width * height * 4);
    m_IsDirty = true;

    if (type == PET_VIEW) {
        ++m_PaintSamples;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - m_LastPaintSample;
        if (elapsed.count() >= 0.5) {
            m_PaintFps = static_cast<double>(m_PaintSamples) / elapsed.count();
            m_PaintSamples = 0;
            m_LastPaintSample = now;
        }
    }
}

void CefRenderHandlerImpl::GetTextureData(std::vector<uint8_t>& data, int& width, int& height) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    width = m_Width;
    height = m_Height;
    data.resize(m_Buffer.size());
    
    // Convert BGRA to RGBA
    for (size_t i = 0; i < m_Buffer.size(); i += 4) {
        data[i] = m_Buffer[i + 2];     // R
        data[i + 1] = m_Buffer[i + 1]; // G
        data[i + 2] = m_Buffer[i];     // B
        data[i + 3] = m_Buffer[i + 3]; // A
    }
}

double CefRenderHandlerImpl::GetPaintFps() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_PaintFps;
}

void CefRenderHandlerImpl::Resize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Width = width;
    m_Height = height;
    m_Buffer.resize(width * height * 4);
}

// CefClientImpl implementation
CefClientImpl::CefClientImpl(CefRefPtr<CefRenderHandlerImpl> renderHandler)
    : m_RenderHandler(renderHandler) {
}

void CefClientImpl::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    m_Browser = browser;
}

void CefClientImpl::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    m_Browser = nullptr;
}
