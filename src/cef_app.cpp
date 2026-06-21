#include "../include/cef_app_impl.h"
#include <iostream>

#ifdef _WIN32
#include <optional>
#include <string>
#include <windows.h>

namespace {
std::optional<std::wstring> GetEnvironmentString(const wchar_t* name) {
    DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return std::nullopt;
    }

    std::wstring buffer(required, L'\0');
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), required);
    if (written == 0) {
        return std::nullopt;
    }

    if (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }

    if (buffer.empty()) {
        return std::nullopt;
    }

    return buffer;
}
}  // namespace
#endif

void CefAppImpl::OnContextInitialized() {
    std::cout << "CEF context initialized" << std::endl;
}

void CefAppImpl::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
    // Intentionally leave GPU-related flags under caller control so Chromium
    // can honor real command-line switches passed to the executable.
#ifdef _WIN32
    if (auto resources_dir = GetEnvironmentString(L"IMGUICEF_CEF_RESOURCES_DIR")) {
        command_line->AppendSwitchWithValue("resources-dir-path", *resources_dir);
    }

    if (auto locales_dir = GetEnvironmentString(L"IMGUICEF_CEF_LOCALES_DIR")) {
        command_line->AppendSwitchWithValue("locales-dir-path", *locales_dir);
    }
#endif
}
