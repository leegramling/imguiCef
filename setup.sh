#!/bin/bash

# Setup script for ImGuiCef project
# Downloads and sets up all dependencies

set -e  # Exit on any error

# CEF Configuration - Update these variables to use different CEF versions
CEF_VERSION="133.4.8+g6b4e4fe+chromium-133.0.6877.0"
CEF_DIR_NAME="cef_binary_133.4.8"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

echo "=== ImGuiCef Project Setup ==="
echo "Project root: $PROJECT_ROOT"

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux64"
    CEF_PLATFORM="linux64"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macosx64"
    CEF_PLATFORM="macosx64"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "win32" ]]; then
    PLATFORM="windows64"
    CEF_PLATFORM="windows64"
else
    echo "Unsupported platform: $OSTYPE"
    exit 1
fi

echo "Detected platform: $PLATFORM"


# Function to download and extract CEF
download_cef() {
    echo "=== Downloading CEF $CEF_VERSION for $CEF_PLATFORM ==="
    
    CEF_FILENAME="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2"
    CEF_URL="https://cef-builds.spotifycdn.com/${CEF_FILENAME}"
    
    if [ ! -d "$CEF_DIR_NAME" ]; then
        echo "Downloading CEF from: $CEF_URL"
        
        if command -v wget >/dev/null 2>&1; then
            wget -O "$CEF_FILENAME" "$CEF_URL"
        elif command -v curl >/dev/null 2>&1; then
            curl -L -o "$CEF_FILENAME" "$CEF_URL"
        else
            echo "Error: Neither wget nor curl found. Please install one of them."
            exit 1
        fi
        
        echo "Extracting CEF..."
        tar -xjf "$CEF_FILENAME"
        rm "$CEF_FILENAME"
        
        # Rename to standard directory name
        if [ -d "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}" ]; then
            mv "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}" "$CEF_DIR_NAME"
        fi
        
        echo "CEF downloaded and extracted to $CEF_DIR_NAME"
    else
        echo "CEF already exists at $CEF_DIR_NAME"
    fi
}

# Function to setup ImGui submodule
setup_imgui_submodule() {
    echo "=== Setting up ImGui submodule ==="
    
    # Initialize and update git submodules
    if [ ! -f "imgui/.git" ] && [ ! -d "imgui/.git" ]; then
        echo "Initializing and updating git submodules..."
        git submodule update --init --recursive
        echo "ImGui submodule initialized successfully"
    else
        echo "ImGui submodule already initialized"
        echo "Updating ImGui submodule..."
        git submodule update --remote imgui
        echo "ImGui submodule updated successfully"
    fi
}

# Function to check and install minimal system dependencies
check_system_deps() {
    echo "=== Checking minimal system dependencies ==="
    
    local missing_deps=()
    
    # Check for essential build tools
    if ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
        missing_deps+=("C++ compiler (gcc or clang)")
    fi
    
    if ! command -v cmake >/dev/null 2>&1; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v git >/dev/null 2>&1; then
        missing_deps+=("git")
    fi
    
    # Check for Vulkan SDK
    if ! command -v vulkaninfo >/dev/null 2>&1; then
        missing_deps+=("Vulkan SDK")
    fi
    
    # Platform-specific checks
    case "$PLATFORM" in
        "linux64")
            # Check for X11 development headers (minimal requirement)
            if ! pkg-config --exists x11 2>/dev/null; then
                missing_deps+=("X11 development libraries")
            fi
            ;;
        "macosx64")
            if ! command -v xcode-select >/dev/null 2>&1; then
                missing_deps+=("Xcode command line tools")
            fi
            ;;
    esac
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        echo "✓ All essential dependencies are available"
        return 0
    else
        echo "✗ Missing dependencies detected:"
        printf "  - %s\n" "${missing_deps[@]}"
        echo ""
        
        case "$PLATFORM" in
            "linux64")
                echo "To install missing dependencies:"
                echo ""
                if command -v apt-get >/dev/null 2>&1; then
                    echo "  sudo apt-get update"
                    echo "  sudo apt-get install build-essential cmake git vulkan-tools libx11-dev"
                elif command -v dnf >/dev/null 2>&1; then
                    echo "  sudo dnf install gcc-c++ cmake git vulkan-tools libX11-devel"
                elif command -v pacman >/dev/null 2>&1; then
                    echo "  sudo pacman -S base-devel cmake git vulkan-tools libx11"
                else
                    echo "  Install the missing packages using your distribution's package manager"
                fi
                ;;
            "macosx64")
                echo "To install missing dependencies:"
                echo "  brew install cmake vulkan-headers vulkan-loader molten-vk"
                echo "  xcode-select --install"
                ;;
            "windows64")
                echo "Please install:"
                echo "  - Visual Studio with C++ support"
                echo "  - CMake"
                echo "  - Vulkan SDK"
                echo "  - Git for Windows"
                ;;
        esac
        
        echo ""
        echo "Note: This project now manages most dependencies automatically:"
        echo "  - GLFW: Downloaded and built by CMake"
        echo "  - ImGui: Git submodule"
        echo "  - CEF: Downloaded by this script"
        echo ""
        
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Function to create build directory and configure
setup_build() {
    echo "=== Setting up build directory ==="
    
    mkdir -p build
    cd build
    
    echo "Configuring CMake..."
    case "$PLATFORM" in
        "linux64"|"macosx64")
            cmake .. -DCMAKE_BUILD_TYPE=Debug
            ;;
        "windows64")
            echo "On Windows, use Visual Studio or run:"
            echo "cmake .. -G \"Visual Studio 16 2019\" -A x64"
            echo "or"
            echo "cmake .. -G \"Visual Studio 17 2022\" -A x64"
            ;;
    esac
    
    cd ..
}

# Function to build the project
build_project() {
    if [ "$PLATFORM" != "windows64" ]; then
        echo "=== Building project ==="
        cd build
        make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        cd ..
        echo "Build completed successfully!"
    else
        echo "On Windows, build using Visual Studio or run:"
        echo "cmake --build build --config Debug"
    fi
}

# Function to verify installation
verify_setup() {
    echo "=== Verifying setup ==="
    
    local errors=0
    
    # Check CEF
    if [ -d "$CEF_DIR_NAME" ] && [ -f "$CEF_DIR_NAME/include/cef_version.h" ]; then
        echo "✓ CEF is properly installed"
    else
        echo "✗ CEF installation issue"
        errors=$((errors + 1))
    fi
    
    # Check ImGui
    if [ -d "imgui" ] && [ -f "imgui/imgui.h" ]; then
        echo "✓ ImGui is properly installed"
    else
        echo "✗ ImGui installation issue"
        errors=$((errors + 1))
    fi
    
    # Check build directory
    if [ -d "build" ]; then
        echo "✓ Build directory created"
    else
        echo "✗ Build directory missing"
        errors=$((errors + 1))
    fi
    
    if [ $errors -eq 0 ]; then
        echo "✓ Setup completed successfully!"
        echo ""
        echo "To build and run:"
        echo "  cd build"
        if [ "$PLATFORM" != "windows64" ]; then
            echo "  make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
            echo "  ./ImGuiCefVulkan"
        else
            echo "  cmake --build . --config Debug"
            echo "  .\\Debug\\ImGuiCefVulkan.exe"
        fi
    else
        echo "✗ Setup completed with $errors error(s)"
        exit 1
    fi
}

# Main execution
main() {
    echo "Starting setup for $PLATFORM platform..."
    
    # Parse command line arguments
    SKIP_DEPS=false
    SKIP_BUILD=false
    
    for arg in "$@"; do
        case $arg in
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [options]"
                echo ""
                echo "This script sets up the ImGuiCef project with minimal system dependencies."
                echo "Most dependencies (GLFW, ImGui) are managed automatically by CMake."
                echo ""
                echo "Options:"
                echo "  --skip-deps    Skip system dependency checking"
                echo "  --skip-build   Skip building the project"
                echo "  --help, -h     Show this help message"
                echo ""
                echo "Minimal system requirements:"
                echo "  - C++ compiler (gcc/clang/MSVC)"
                echo "  - CMake 3.20+"
                echo "  - Git"
                echo "  - Vulkan SDK"
                echo "  - X11 development libraries (Linux only)"
                exit 0
                ;;
        esac
    done
    
    # Run setup steps
    if [ "$SKIP_DEPS" = false ]; then
        check_system_deps
    fi
    
    download_cef
    setup_imgui_submodule
    setup_build
    
    if [ "$SKIP_BUILD" = false ]; then
        build_project
    fi
    
    verify_setup
}

# Run main function with all arguments
main "$@"