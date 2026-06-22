#include "../include/cef_app_impl.h"
#include <iostream>

#ifdef _WIN32
#include <filesystem>
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

std::filesystem::path GetExecutableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                     static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}
}  // namespace
#endif

void CefAppImpl::OnContextInitialized() {
    std::cout << "CEF context initialized" << std::endl;
}

void CefAppImpl::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
    if (process_type.empty() && !command_line->HasSwitch("show-fps-counter")) {
        command_line->AppendSwitch("show-fps-counter");
    }

    if (process_type.empty() && command_line->HasSwitch("unlimited-fps")) {
        command_line->AppendSwitch("disable-frame-rate-limit");
        command_line->AppendSwitch("disable-gpu-vsync");
    }

#ifdef _WIN32
    const std::filesystem::path executable_dir = GetExecutableDirectory();
    const std::filesystem::path development_cef_dir = executable_dir / "cef";
    const std::filesystem::path default_resources_dir =
        std::filesystem::exists(development_cef_dir / "resources.pak")
            ? development_cef_dir
            : executable_dir;
    const std::filesystem::path resources_dir =
        GetEnvironmentString(L"IMGUICEF_CEF_RESOURCES_DIR")
            .value_or(default_resources_dir.wstring());
    const std::filesystem::path locales_dir =
        GetEnvironmentString(L"IMGUICEF_CEF_LOCALES_DIR")
            .value_or((resources_dir / "locales").wstring());

    // CefSettings applies to the browser process. Explicit switches ensure the
    // renderer and GPU subprocesses use the same development resource layout.
    command_line->AppendSwitchWithValue("resources-dir-path", resources_dir.wstring());
    command_line->AppendSwitchWithValue("locales-dir-path", locales_dir.wstring());
#endif

#if defined(__linux__)
    // The Chromium zygote can fail to handshake in WSL, which prevents the GPU
    // process from launching and causes CEF to abort after repeated retries.
    if (process_type.empty() && !command_line->HasSwitch("no-zygote")) {
        command_line->AppendSwitch("no-zygote");
    }
#endif
}
