# Windows Build Instructions

## Changes Made for Windows Compilation

The CMake configuration has been updated to support Windows compilation alongside the existing Linux support. The following changes were made:

### 1. Platform-Specific Library Linking

**Main CMakeLists.txt**:
- Moved Linux-specific libraries (`dl`, `X11`) into platform-conditional blocks
- Added Windows-specific system libraries:
  - `user32`, `gdi32`, `shell32`, `ole32`, `oleaut32`, `uuid`, `winmm`
  - `advapi32`, `kernel32`, `ws2_32`, `psapi`, `dbghelp`, `comctl32`

**tests/CMakeLists.txt**:
- Made `dl` library linking conditional for Linux only

### 2. CEF Configuration Improvements

**Dynamic CEF Binary Directory**:
- CEF binary directory now adapts based on build configuration (Debug/Release)
- Debug builds use `${CEF_ROOT}/Debug/`, Release builds use `${CEF_ROOT}/Release/`
- CEF sandbox library selection is now configuration-aware

**CEF Sandbox Handling**:
- CEF sandbox is **disabled by default** to avoid linking issues
- Can be enabled with `-DUSE_CEF_SANDBOX=ON` if needed for production
- Sandbox linking often causes issues with MSVC - disabling is recommended for development

**Windows Resource Handling**:
- Improved DLL copying from appropriate CEF binary directory
- Added V8 snapshot file copying for Windows
- Enhanced resource verification (icudtl.dat check)

### 3. Windows-Specific Compile Definitions

Added essential Windows compile definitions for both main executable and CEF wrapper:
- `WIN32_LEAN_AND_MEAN` - Reduces Windows header bloat
- `NOMINMAX` - Prevents min/max macro conflicts
- `_CRT_SECURE_NO_WARNINGS` - Disables MSVC security warnings
- `_UNICODE` and `UNICODE` - Enables Unicode support

**MSVC-Specific Compiler Flags**:
- `/permissive-` - Enables conformance mode
- `/W3` - Warning level 3
- `/wd4996` - Disables deprecated function warnings

### 4. Test Environment Configuration

**Environment Variables**:
- Linux: Uses `LD_LIBRARY_PATH`
- Windows: Uses `PATH` with semicolon separators

## Prerequisites for Windows

### Required Tools
1. **Visual Studio 2019 or later** or **Visual Studio Build Tools**
   - C++ desktop development workload
   - Windows 10/11 SDK
   - CMake tools for Visual Studio

2. **CMake 3.20 or later**
   - Can be installed via Visual Studio installer
   - Or download from https://cmake.org/

3. **Vulkan SDK**
   - Download from https://vulkan.lunarg.com/
   - Install with validation layers

### CEF Binary Requirements
Download the appropriate CEF binary distribution for Windows:
- Visit https://cef-builds.spotifycdn.com/index.html
- Download CEF binary for Windows (matching your target architecture)
- Extract to `cef_binary_<version>` directory in project root
- Update `CMakeLists.txt` line 29 if version differs from 133.4.8

## Building on Windows

### Option 1: Visual Studio IDE
1. Open project folder in Visual Studio
2. Visual Studio will automatically detect CMakeLists.txt
3. Select configuration (Debug/Release) and architecture (x64/x86)
4. Build → Build All

### Option 2: Command Line with MSVC
```cmd
# Open "Developer Command Prompt for VS" or "x64 Native Tools Command Prompt"
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Option 2a: Enable CEF Sandbox (Optional)
If you need CEF sandbox enabled (not recommended for development):
```cmd
cmake .. -G "Visual Studio 16 2019" -A x64 -DUSE_CEF_SANDBOX=ON
cmake --build . --config Release
```

### Option 3: Command Line with Ninja
```cmd
# Open "Developer Command Prompt for VS"
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

## Running the Application

### Windows Executable Location
After building, the executable will be in:
- Visual Studio: `build/Release/ImGuiCefVulkan.exe` or `build/Debug/ImGuiCefVulkan.exe`
- Command line: `build/ImGuiCefVulkan.exe`

### Running
```cmd
# For Visual Studio builds (Debug/Release configurations)
cd build/Debug
ImGuiCefVulkan.exe

# Or for Release builds
cd build/Release
ImGuiCefVulkan.exe

# For single-config generators (Ninja, Makefiles)
cd build
ImGuiCefVulkan.exe
```

**Important**: All required CEF libraries and resources are automatically copied to the executable's directory during build, so you can run directly from the executable's location.

## Testing on Windows

```cmd
cd build
ctest -C Release
```

Or run the test executable directly:
```cmd
cd build
test_cef_initialize.exe
```

## Troubleshooting

### Common Issues

1. **CEF Sandbox Linking Errors**
   - CEF sandbox is disabled by default to avoid MSVC linking issues
   - If you need sandbox, use `-DUSE_CEF_SANDBOX=ON` but expect potential linking problems
   - For development, the disabled sandbox is sufficient and recommended

2. **CEF Library Not Found**
   - Ensure CEF binary path in CMakeLists.txt line 29 matches your CEF version
   - Verify CEF binary distribution contains both Debug and Release folders

2. **Missing DLLs at Runtime**
   - Ensure you're running from the build directory
   - Check that all CEF DLLs were copied to the build directory

3. **Vulkan Initialization Failed**
   - Install latest graphics drivers
   - Verify Vulkan SDK installation with `vulkaninfo.exe`

4. **MSVC Compilation Errors**
   - Ensure Visual Studio has C++ desktop development workload
   - Try updating to latest Visual Studio version

### Verification Steps
After successful build, the executable's directory (`build/Debug/` or `build/Release/`) should contain:

**Executable:**
- `ImGuiCefVulkan.exe`

**Core CEF Libraries:**
- `libcef.dll` - Main CEF library
- `d3dcompiler_47.dll` - DirectX shader compiler  
- `libEGL.dll` - OpenGL ES implementation
- `libGLESv2.dll` - OpenGL ES v2 implementation
- `vulkan-1.dll` - Vulkan loader (if present)

**CEF Resources:**
- `icudtl.dat` - ICU internationalization data
- `chrome_100_percent.pak` - UI resources (100% scale)
- `chrome_200_percent.pak` - UI resources (200% scale) 
- `resources.pak` - Main resource bundle
- `locales/` directory - Language pack files (*.pak)

**V8 Engine Files (if present):**
- `snapshot_blob.bin` - V8 JavaScript engine snapshot
- `v8_context_snapshot.bin` - V8 context snapshot

**Configuration Files:**
- `vk_swiftshader_icd.json` - Vulkan Swiftshader configuration

**Note**: All files are automatically copied to the same directory as the executable via POST_BUILD commands.

## Architecture Notes

The Windows build maintains the same architecture as the Linux version:
- **Vulkan Renderer**: Cross-platform Vulkan 1.0 implementation
- **CEF Integration**: Offscreen rendering with BGRA→RGBA conversion
- **ImGui Interface**: Platform-agnostic UI rendering
- **Resource Management**: Platform-specific file copying and linking

All core functionality remains identical between Linux and Windows builds.