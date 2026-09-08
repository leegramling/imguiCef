#include "../include/cef_client_impl.h"
#include <cstring>
#include <algorithm>
#include <iostream>

#if defined(__linux__)
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#endif

namespace {
const char* CefColorTypeName(cef_color_type_t format) {
    switch (format) {
    case CEF_COLOR_TYPE_RGBA_8888:
        return "RGBA_8888";
    case CEF_COLOR_TYPE_BGRA_8888:
        return "BGRA_8888";
    default:
        return "unknown";
    }
}
}  // namespace

// CefRenderHandlerImpl implementation
CefRenderHandlerImpl::CefRenderHandlerImpl(int width, int height)
    : m_Width(width),
      m_Height(height),
      m_IsDirty(false),
      m_PaintFps(0.0),
      m_PaintSamples(0),
      m_AcceleratedPaintFrames(0),
      m_LoggedAcceleratedFallback(false),
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

void CefRenderHandlerImpl::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
    ZoneScoped;
    ++m_AcceleratedPaintFrames;

#if defined(__linux__)
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    if (type != PET_VIEW || width <= 0 || height <= 0 || info.plane_count != 1 ||
        (info.format != CEF_COLOR_TYPE_RGBA_8888 && info.format != CEF_COLOR_TYPE_BGRA_8888)) {
        if (!m_LoggedAcceleratedFallback) {
            std::cout << "CEF accelerated paint is enabled, but this frame layout is unsupported: "
                      << "type=" << type
                      << " planes=" << info.plane_count
                      << " size=" << width << "x" << height
                      << " format=" << CefColorTypeName(info.format)
                      << std::endl;
            m_LoggedAcceleratedFallback = true;
        }
        return;
    }

    const auto& plane = info.planes[0];
    const size_t row_bytes = static_cast<size_t>(width) * 4;
    const size_t required_size =
        plane.offset + static_cast<uint64_t>(height - 1) * plane.stride + row_bytes;
    if (plane.fd < 0 || plane.stride < row_bytes || plane.size < required_size) {
        if (!m_LoggedAcceleratedFallback) {
            std::cout << "CEF accelerated paint frame cannot be copied safely: "
                      << "fd=" << plane.fd
                      << " stride=" << plane.stride
                      << " plane_size=" << plane.size
                      << " required=" << required_size
                      << " format=" << CefColorTypeName(info.format)
                      << std::endl;
            m_LoggedAcceleratedFallback = true;
        }
        return;
    }

    void* mapped = mmap(nullptr, plane.size, PROT_READ, MAP_SHARED, plane.fd, 0);
    if (mapped == MAP_FAILED) {
        if (!m_LoggedAcceleratedFallback) {
            std::cout << "CEF accelerated paint mmap failed; falling back to CPU OnPaint when available. errno="
                      << errno << std::endl;
            m_LoggedAcceleratedFallback = true;
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (width != m_Width || height != m_Height) {
            m_Width = width;
            m_Height = height;
            m_Buffer.resize(static_cast<size_t>(width) * height * 4);
        }

        const auto* src_base =
            static_cast<const uint8_t*>(mapped) + static_cast<size_t>(plane.offset);
        for (int row = 0; row < height; ++row) {
            uint8_t* dst = m_Buffer.data() + static_cast<size_t>(row) * row_bytes;
            const uint8_t* src = src_base + static_cast<size_t>(row) * plane.stride;
            if (info.format == CEF_COLOR_TYPE_BGRA_8888) {
                std::memcpy(dst, src, row_bytes);
            } else {
                for (size_t x = 0; x < row_bytes; x += 4) {
                    dst[x] = src[x + 2];
                    dst[x + 1] = src[x + 1];
                    dst[x + 2] = src[x];
                    dst[x + 3] = src[x + 3];
                }
            }
        }

        m_IsDirty = true;
        ++m_PaintSamples;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - m_LastPaintSample;
        if (elapsed.count() >= 0.5) {
            m_PaintFps = static_cast<double>(m_PaintSamples) / elapsed.count();
            m_PaintSamples = 0;
            m_LastPaintSample = now;
        }
    }

    munmap(mapped, plane.size);
#else
    if (!m_LoggedAcceleratedFallback) {
        std::cout << "CEF accelerated paint is enabled, but shared texture import is not implemented on this platform."
                  << std::endl;
        m_LoggedAcceleratedFallback = true;
    }
#endif
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
