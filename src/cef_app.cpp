#include "../include/cef_app_impl.h"
#include <iostream>

void CefAppImpl::OnContextInitialized() {
    std::cout << "CEF context initialized" << std::endl;
}

void CefAppImpl::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
    // Intentionally leave GPU-related flags under caller control so Chromium
    // can honor real command-line switches passed to the executable.
}
