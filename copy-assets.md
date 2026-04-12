# CEF Runtime Asset Layout

The Windows build now stages non-DLL CEF runtime assets under the executable folder:

- `build-cef143/Debug/cef`
- `build-cef143/Release/cef`

The executable remains in:

- `build-cef143/Debug/ImGuiCefVulkan.exe`
- `build-cef143/Release/ImGuiCefVulkan.exe`

## Assets copied into `Debug/cef`

These non-DLL assets are copied into `build-cef143/Debug/cef` after build:

- `chrome_100_percent.pak`
- `chrome_200_percent.pak`
- `resources.pak`
- `locales/`
- `icudtl.dat`
- `v8_context_snapshot.bin`
- `vk_swiftshader_icd.json`

If a CEF package also includes `snapshot_blob.bin`, it will be copied there too.

The copy is performed by CMake, not by a separate script.

Specifically, `CMakeLists.txt` adds Windows `POST_BUILD` commands on the
`ImGuiCefVulkan` target:

- `cmake -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/cef"`
- `cmake -E copy_directory "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/cef"`
- `cmake -E copy_if_different ... "$<TARGET_FILE_DIR:${PROJECT_NAME}>/cef/..."`

So the `cef` folder is created and populated as part of the normal build step for
`Debug` and `Release`.

## DLLs

The native DLLs are still staged in the build root, not in `Debug/cef`:

- `build-cef143/libcef.dll`
- `build-cef143/chrome_elf.dll`
- `build-cef143/d3dcompiler_47.dll`
- `build-cef143/dxcompiler.dll`
- `build-cef143/dxil.dll`
- `build-cef143/libEGL.dll`
- `build-cef143/libGLESv2.dll`
- `build-cef143/vk_swiftshader.dll`
- `build-cef143/vulkan-1.dll`

`src/main.cpp` now adds the parent build directory to the Windows DLL search path with `SetDllDirectoryW(...)`, so the `Debug` and `Release` executables can keep using those root-level DLLs.

## Current experiment result

The `Debug\cef` asset layout works for CEF resource lookup, but launching
`Debug\ImGuiCefVulkan.exe` directly still fails before `main()` with
`0xC0000135` (`-1073741515`) if the required DLLs are not already discoverable at
process startup.

That is a Windows loader constraint, not a CEF `resources_dir_path` /
`locales_dir_path` issue. The executable does run when launched in a context where
the build-root DLLs are discoverable.

## CEF paths used at runtime on Windows

The app now resolves these paths from the executable location and passes absolute paths into CEF:

- `root_cache_path = <exe-dir>/cef_cache`
- `log_file = <exe-dir>/debug.log`
- `resources_dir_path = <exe-dir>/cef`
- `locales_dir_path = <exe-dir>/cef/locales`

This means the resource packs and locales no longer need to live in the build root or current working directory.

## CEF 143 support

CEF 143 supports both of these on Windows when set to absolute paths:

- `CefSettings.resources_dir_path`
- `CefSettings.locales_dir_path`

That support is documented in the vendored SDK header:

- `cef_binary_143.0.14+gdd46a37+chromium-143.0.7499.193_windows64/include/internal/cef_types.h`

This only applies to resource data lookup. DLL loading is still a separate Windows loader concern.
