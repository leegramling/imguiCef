#include "../include/cef_client_impl.h"
#include <cstring>
#include <algorithm>
#include <iostream>

// CefRenderHandlerImpl implementation
CefRenderHandlerImpl::CefRenderHandlerImpl(int width, int height)
    : m_Width(width), m_Height(height), m_IsDirty(false) {
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (width != m_Width || height != m_Height) {
        m_Width = width;
        m_Height = height;
        m_Buffer.resize(width * height * 4);
    }
    
    // Copy the entire buffer (BGRA format)
    std::memcpy(m_Buffer.data(), buffer, width * height * 4);
    m_IsDirty = true;
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