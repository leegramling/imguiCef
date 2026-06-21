#include "../include/cef_app_impl.h"
#include <iostream>

void CefAppImpl::OnContextInitialized() {
    std::cout << "CEF context initialized" << std::endl;
}

void CefAppImpl::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
#if defined(__linux__)
    // The Chromium zygote can fail to handshake in WSL, which prevents the GPU
    // process from launching and causes CEF to abort after repeated retries.
    if (process_type.empty() && !command_line->HasSwitch("no-zygote")) {
        command_line->AppendSwitch("no-zygote");
    }
#endif
}
