#include "../include/cef_app_impl.h"
#include <iostream>

void CefAppImpl::OnContextInitialized() {
    std::cout << "CEF context initialized" << std::endl;
}

void CefAppImpl::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
    // Disable GPU acceleration to avoid OpenGL library issues
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
    command_line->AppendSwitch("disable-software-rasterizer");
}