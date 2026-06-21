# Settings

This repo currently configures most runtime behavior in source code. Command-line
arguments are still passed through to Chromium/CEF, so Chromium switches can be
used at launch.

## Current Settings

### ImGuiCefVulkan

Defined in `src/main.cpp`.

| Setting | Current value | Purpose |
| --- | --- | --- |
| `CefSettings.windowless_rendering_enabled` | `true` | Enables CEF offscreen rendering so browser pixels can be uploaded into a Vulkan texture. |
| `CefSettings.no_sandbox` | `true` | Disables the Chromium sandbox for easier local development. |
| `CefSettings.log_severity` | `LOGSEVERITY_INFO` | Writes informational CEF logs. |
| `CefSettings.command_line_args_disabled` | `false` | Allows Chromium/CEF command-line switches passed to `ImGuiCefVulkan`. |
| `CefSettings.root_cache_path` | `<run directory>/cef_cache` on Linux, `<exe directory>/cef_cache` on Windows | CEF cache/profile root. |
| `CefSettings.log_file` | `<run directory>/debug.log` on Linux, `<exe directory>/debug.log` on Windows | CEF log output file. |
| `CefSettings.resources_dir_path` | `<run directory>` on Linux, `<exe directory>/cef` on Windows | CEF resource bundle directory. |
| `CefSettings.locales_dir_path` | `<run directory>/locales` on Linux, `<exe directory>/cef/locales` on Windows | CEF locale bundle directory. |
| `CefBrowserSettings.windowless_frame_rate` | `60` | Maximum CEF offscreen paint rate. This is already set to 60 FPS. |
| GLFW client API | `GLFW_NO_API` | Creates a window without OpenGL so Vulkan owns presentation. |
| Renderer API | Vulkan | `VulkanRenderer` creates a Vulkan instance/device/swapchain and ImGui uses `imgui_impl_vulkan`. |
| Vulkan API version | `VK_API_VERSION_1_0` | Vulkan instance API version requested by the renderer. |
| Vulkan swapchain present mode | `VK_PRESENT_MODE_FIFO_KHR` | V-sync style presentation mode; usually capped by display refresh. |
| Browser size | `800x600` initial | Initial CEF render handler/browser texture size. |
| Default URL | `https://www.google.com` | Initial page loaded by the browser. |

### cefForms

Defined in `src/cef_forms_main.cpp`.

| Setting | Current value | Purpose |
| --- | --- | --- |
| `CefSettings.windowless_rendering_enabled` | `true` | Enables CEF offscreen rendering. |
| `CefSettings.no_sandbox` | `true` | Disables the Chromium sandbox for easier local development. |
| `CefBrowserSettings.windowless_frame_rate` | `60` | Maximum CEF offscreen paint rate. This is also already set to 60 FPS. |
| GLFW client API | `GLFW_NO_API` | Uses a non-OpenGL window for Vulkan presentation. |
| Renderer API | Vulkan | Shares the same Vulkan renderer path. |

## Launch Settings

Because `command_line_args_disabled` is `false`, Chromium/CEF switches can be
passed directly to the executable.

Useful local Linux launch command from this environment:

```bash
cd build
./ImGuiCefVulkan --no-zygote --disable-gpu --disable-gpu-compositing
```

Observed behavior:

| Switch | When to use it |
| --- | --- |
| `--no-zygote` | Use when Chromium subprocess startup fails with zygote or GPU-process launch errors. |
| `--disable-gpu` | Disables Chromium's internal GPU acceleration. This does not disable the app's Vulkan renderer. |
| `--disable-gpu-compositing` | Forces Chromium/CEF page compositing onto the CPU path. The final app window still renders with Vulkan. |
| `--single-process` | Diagnostic fallback only. It avoided subprocess failures here, but Chromium warns that single-process mode is not a normal production mode. |
| `--gpu-test` | App-specific helper in `src/main.cpp`; changes the initial URL to `chrome://gpu`. |

## FPS

CEF offscreen paint FPS is already set to 60:

```cpp
CefBrowserSettings browser_settings;
browser_settings.windowless_frame_rate = 60;
```

That setting controls the maximum rate at which CEF calls the offscreen render
handler. Actual frame rate can still be lower if the page is idle, CEF cannot
produce frames quickly enough, or the Vulkan presentation path is capped by
`VK_PRESENT_MODE_FIFO_KHR`/display refresh.

CEF also supports changing the offscreen browser frame rate after creation with:

```cpp
browser->GetHost()->SetWindowlessFrameRate(60);
```

The current app does not expose a UI or config-file control for this; changing
the hardcoded value requires editing `src/main.cpp` and/or `src/cef_forms_main.cpp`.

## Vulkan

The app already uses Vulkan for presentation and ImGui rendering:

| Vulkan area | Current behavior |
| --- | --- |
| Window creation | GLFW window is created with `GLFW_NO_API`, so no OpenGL context is created. |
| Vulkan instance | Created in `VulkanRenderer::CreateInstance()`. |
| Vulkan device | First enumerated physical device is selected, then a graphics queue is used. |
| Swapchain | Uses `VK_FORMAT_B8G8R8A8_UNORM` and `VK_PRESENT_MODE_FIFO_KHR`. |
| CEF texture upload | CEF BGRA/RGBA frame data is uploaded into Vulkan images and displayed through `ImGui_ImplVulkan_AddTexture`. |

The Chromium/CEF switches `--disable-gpu` and `--disable-gpu-compositing` only
affect Chromium's internal page rendering path. They do not switch this app away
from Vulkan.

## Available Settings To Expose Later

These are useful candidates for a config file, environment variable, or command
line parser if runtime control is needed:

| Candidate setting | Current source value | Suggested runtime name |
| --- | --- | --- |
| CEF offscreen FPS | `60` | `--cef-fps=60` |
| Initial URL | `https://www.google.com` | `--url=https://example.com` |
| Initial browser width | `800` | `--browser-width=800` |
| Initial browser height | `600` | `--browser-height=600` |
| CEF cache directory | `cef_cache` | `--cef-cache-path=...` |
| CEF log path | `debug.log` | `--cef-log-file=...` |
| CEF log severity | `LOGSEVERITY_INFO` | `--cef-log-severity=info` |
| Chromium zygote workaround | launch switch only | `--no-zygote` |
| Chromium GPU workaround | launch switch only | `--disable-gpu --disable-gpu-compositing` |
| Vulkan present mode | `VK_PRESENT_MODE_FIFO_KHR` | `--present-mode=fifo/mailbox/immediate` |
| Vulkan device selection | first enumerated device | `--vulkan-device=<index-or-name>` |
