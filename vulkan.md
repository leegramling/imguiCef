# Vulkan Flags

This project has two separate Vulkan concerns:

- The application renderer uses Vulkan directly for the native window, ImGui, swapchain presentation, and the final browser texture.
- Chromium/CEF has its own GPU process and graphics stack. The flags below ask Chromium/CEF to use Vulkan internally as well.

## CEF/Chromium Flags

Set these Chromium switches before `CefInitialize`, usually in `CefApp::OnBeforeCommandLineProcessing`:

```cpp
command_line->AppendSwitch("use-vulkan");
command_line->AppendSwitchWithValue("use-angle", "vulkan");
command_line->AppendSwitchWithValue("enable-features", "Vulkan,VulkanFromANGLE");
```

Equivalent command-line form:

```bash
./ImGuiCefVulkan \
  --use-vulkan \
  --use-angle=vulkan \
  --enable-features=Vulkan,VulkanFromANGLE
```

Current project behavior:

- `src/cef_app.cpp` adds `use-vulkan` automatically if the caller did not pass it.
- `src/cef_app.cpp` adds `use-angle=vulkan` automatically if the caller did not pass `use-angle`.
- `src/cef_app.cpp` appends `Vulkan` and `VulkanFromANGLE` to any existing `enable-features` value.

## Linux Runtime Flags

On Linux, this project also adds:

```cpp
command_line->AppendSwitch("no-zygote");
```

Equivalent command-line form:

```bash
./ImGuiCefVulkan --no-zygote
```

This is a process startup workaround for WSL/container-like environments. It is not a Vulkan flag.

## Flags To Avoid

These Chromium switches disable the CEF GPU rendering path:

```bash
--disable-gpu
--disable-gpu-compositing
--disable-software-rasterizer
```

Use them only for headless/init tests. They do not disable this app's native Vulkan renderer, but they do prevent Chromium/CEF from using its GPU path.

## App Vulkan Path

No CEF flag is needed for the application's own renderer. The app already:

- creates a GLFW window with `GLFW_NO_API`;
- initializes Vulkan in `VulkanRenderer`;
- renders ImGui through `imgui_impl_vulkan`;
- uploads CEF frame data into a Vulkan image and displays it with `ImGui_ImplVulkan_AddTexture`.

## Shared Textures

The Vulkan flags above do not by themselves enable shared textures.

CEF shared-texture rendering additionally requires:

```cpp
CefWindowInfo window_info;
window_info.SetAsWindowless(0);
window_info.shared_texture_enabled = true;
```

and a `CefRenderHandler::OnAcceleratedPaint` implementation. Without shared textures, CEF uses `OnPaint` and the app uploads CPU pixel buffers into Vulkan textures.

On Linux, CEF accelerated paint depends on the runtime being able to allocate GBM/DRM-backed buffers. In environments without GBM/DRM support, CEF may log errors such as:

```text
drmGetDevices2() has not found any devices
dri3 extension not supported
Can't create buffer -- gbm device is missing
```

In that case, the Vulkan flags can still be present, but shared-texture rendering will not work until the graphics environment supports those buffers.
