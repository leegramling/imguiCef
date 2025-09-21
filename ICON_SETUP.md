# Windows 11 Taskbar Icon Setup for ImGuiCefVulkan

This project includes two approaches for Windows 11 taskbar icon display:
1. **Embedded Resource (Recommended)** - Icon compiled into the executable
2. **Runtime Loading** - Dynamic icon loading via GLFW

## Files Added

### Embedded Resource Approach (Recommended for Windows)
- `resources/app.rc` - Windows resource file with embedded icon
- `resources/resource.h` - Resource definitions
- `resources/browser.ico` - Embedded icon file
- Updated `CMakeLists.txt` to compile resources on Windows

### Runtime Loading Approach (Cross-platform)
- `create_icons.py` - Python script to generate icons in multiple sizes
- `icons/` directory containing:
  - `browser_16x16.png` - Small icon for title bars
  - `browser_32x32.png` - Standard taskbar icon
  - `browser_48x48.png` - Large icon for high-DPI displays
  - `browser.ico` - Windows ICO file with multiple sizes
- `include/icon_loader.h` - Header file for the IconLoader class
- `src/icon_loader.cpp` - Implementation of icon loading functionality

## How It Works

### 1. Embedded Resource Approach (Recommended)

Windows automatically uses embedded icon resources for:
- **Taskbar icons** - What appears in the Windows taskbar
- **Title bar icons** - Small icon in window title bar
- **Alt+Tab switcher** - Icon shown when switching between applications
- **Task Manager** - Icon displayed in process list
- **File associations** - Icon for the executable file

**How it works:**
- Icon is compiled directly into the `.exe` file during build
- Windows automatically detects and uses `IDI_ICON1` resource
- No runtime loading required - works immediately
- Guaranteed to work on all Windows versions

### 2. Runtime Loading Approach (Cross-platform)

For cross-platform compatibility and dynamic icon changes:
1. **Multiple Icon Sizes**: Provides 16x16, 32x32, and 48x48 icons for different display contexts
2. **GLFW Integration**: Uses `glfwSetWindowIcon()` with multiple sizes
3. **Windows-Specific Handling**: Includes Windows API calls to force icon refresh
4. **Fallback System**: Creates icons programmatically if PNG files aren't available

### Key Features
- **Automatic Icon Detection**: Scans the `icons/` directory for properly named files
- **Size Validation**: Ensures only supported icon sizes (16x16, 32x32, 48x48) are loaded
- **Memory Management**: Proper cleanup of icon data
- **Cross-Platform**: Works on Windows, Linux, and macOS
- **Error Handling**: Graceful fallback if icons can't be loaded

## Integration

The icon loading is integrated into the main application in `src/main.cpp`:

```cpp
#include "../include/icon_loader.h"

// In InitializeWindow():
if (!IconLoader::LoadAndSetWindowIcon(m_Window, "icons")) {
    std::cerr << "Warning: Failed to load window icon" << std::endl;
}
```

## Usage in Other Projects

To use this icon system in another project:

1. **Copy Files**:
   - `include/icon_loader.h`
   - `src/icon_loader.cpp`
   - `create_icons.py`

2. **Add to CMakeLists.txt**:
   ```cmake
   set(SOURCES
       # ... your existing sources
       src/icon_loader.cpp
   )
   ```

3. **Include and Use**:
   ```cpp
   #include "icon_loader.h"

   // After creating your GLFW window:
   IconLoader::LoadAndSetWindowIcon(window, "path/to/icons");
   ```

4. **Generate Icons**:
   ```bash
   python3 create_icons.py
   ```

## Windows 11 Specific Notes

Windows 11 can be particular about taskbar icons. This implementation addresses common issues:

- **Icon Refresh**: Forces window hide/show to refresh the taskbar icon
- **Multiple Sizes**: Provides all standard Windows icon sizes
- **Proper Format**: Uses RGBA format that Windows expects
- **Memory Layout**: Ensures correct pixel data ordering

## Troubleshooting

If icons still don't appear on Windows 11:

1. **Verify Icon Files**: Ensure `icons/` directory contains the PNG files
2. **Check Permissions**: Make sure the application can read the icon files
3. **DPI Settings**: High-DPI displays may require the 48x48 icon
4. **Windows Cache**: Try restarting the application or clearing icon cache
5. **Build Configuration**: Ensure `icon_loader.cpp` is included in the build

## Customization

To use custom icons:

1. Replace the generated icons with your own 16x16, 32x32, and 48x48 PNG files
2. Keep the naming convention: `browser_WIDTHxHEIGHT.png`
3. Or modify the `GetIconFilenames()` function for different naming

The icon loader is designed to be flexible and reusable across different projects requiring Windows 11 taskbar compatibility.