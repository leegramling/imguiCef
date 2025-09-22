# Windows Icon Setup for ImGuiCefVulkan

This project uses embedded Windows resources for proper taskbar icon display on Windows 11.

## Files Added

### Embedded Resource Implementation
- `resources/app.rc` - Windows resource file with embedded icon
- `resources/resource.h` - Resource definitions
- `resources/browser.ico` - Rocket-themed icon file (embedded in executable)
- `create_icons.py` - Python script to generate the rocket icon
- Updated `CMakeLists.txt` to compile resources on Windows builds

## How It Works

### Embedded Resource Approach

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
- No external files needed - completely self-contained

### Rocket Icon Design

The embedded icon features:
- **Rocket Shape**: Classic rocket with pointed nose cone and stabilizer fins
- **Fire Trail**: Multi-layered exhaust with orange-to-yellow gradient
- **15° Right Tilt**: Dynamic angled appearance
- **Multi-Size Support**: 16x16, 32x32, and 48x48 sizes in single ICO file
- **Progressive Detail**: More details visible at larger sizes

## Integration

No code integration required! The icon is automatically embedded during Windows builds:

1. **CMake detects Windows** and includes `resources/app.rc`
2. **Resource compiler** embeds the icon into the executable
3. **Windows automatically** uses the embedded icon for taskbar/title bar

## Usage in Other Projects

To use this embedded icon system in another project:

1. **Copy Files**:
   - `resources/app.rc`
   - `resources/resource.h`
   - `create_icons.py` (to generate your own icon)

2. **Add to CMakeLists.txt**:
   ```cmake
   # Windows resource file for embedded icon
   if(WIN32)
       set(RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
       if(EXISTS "${RESOURCE_FILE}")
           list(APPEND SOURCES ${RESOURCE_FILE})
       endif()
   endif()
   ```

3. **Generate Your Icon**:
   ```bash
   python3 create_icons.py
   cp icons/browser.ico resources/
   ```

4. **Build**:
   The icon will be automatically embedded in your Windows executable!

## Benefits of Embedded Resources

✅ **No external files** - Icon is baked into the executable
✅ **Automatic detection** - Windows handles everything automatically
✅ **No path issues** - No need to worry about working directories
✅ **No runtime code** - Zero performance impact
✅ **Industry standard** - This is how professional Windows applications handle icons
✅ **Future-proof** - Works with all Windows versions

## Customization

To create your own rocket icon design:

1. **Edit the Python script**: Modify `create_icons.py` to change colors, shape, or design
2. **Regenerate**: Run `python3 create_icons.py` to create new icons
3. **Copy to resources**: `cp icons/browser.ico resources/`
4. **Rebuild**: The new icon will be embedded automatically

The embedded resource approach is the clean, professional solution for Windows application icons.