#!/bin/bash

# Setup script for ImGuiCef project
# Downloads and sets up all dependencies

set -e  # Exit on any error

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

# CEF version - you can update this to use newer versions
CEF_VERSION="105.3.39+g0bb4138+chromium-105.0.5195.127"
CEF_DIR_NAME="cef_binary_105.3.39"

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

# Function to install system dependencies
install_system_deps() {
    echo "=== Installing system dependencies ==="
    
    case "$PLATFORM" in
        "linux64")
            echo "Installing Linux dependencies..."
            
            # Check if we're on a Debian/Ubuntu system
            if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get update
                sudo apt-get install -y \
                    build-essential \
                    cmake \
                    pkg-config \
                    libglfw3-dev \
                    libvulkan-dev \
                    vulkan-tools \
                    vulkan-validationlayers-dev \
                    libx11-dev \
                    libxrandr-dev \
                    libxinerama-dev \
                    libxcursor-dev \
                    libxi-dev \
                    libnss3-dev \
                    libatk-bridge2.0-dev \
                    libgtk-3-dev \
                    libxss1 \
                    libasound2-dev
            
            # Check if we're on a Red Hat/Fedora system
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y \
                    gcc-c++ \
                    cmake \
                    pkgconfig \
                    glfw-devel \
                    vulkan-devel \
                    vulkan-tools \
                    vulkan-validation-layers-devel \
                    libX11-devel \
                    libXrandr-devel \
                    libXinerama-devel \
                    libXcursor-devel \
                    libXi-devel \
                    nss-devel \
                    at-spi2-atk-devel \
                    gtk3-devel \
                    libXScrnSaver \
                    alsa-lib-devel
            
            elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --needed \
                    base-devel \
                    cmake \
                    glfw-x11 \
                    vulkan-devel \
                    vulkan-tools \
                    vulkan-validation-layers \
                    libx11 \
                    libxrandr \
                    libxinerama \
                    libxcursor \
                    libxi \
                    nss \
                    at-spi2-atk \
                    gtk3 \
                    libxss \
                    alsa-lib
            else
                echo "Warning: Unsupported Linux distribution. Please install dependencies manually:"
                echo "- Build tools (gcc, cmake, pkg-config)"
                echo "- GLFW3 development libraries"
                echo "- Vulkan SDK and validation layers"
                echo "- X11 development libraries"
                echo "- CEF runtime dependencies (NSS, GTK3, ALSA, etc.)"
            fi
            ;;
            
        "macosx64")
            echo "Installing macOS dependencies..."
            
            if command -v brew >/dev/null 2>&1; then
                brew install cmake glfw vulkan-headers vulkan-loader molten-vk
                echo "Note: Make sure you have Xcode command line tools installed:"
                echo "xcode-select --install"
            else
                echo "Error: Homebrew not found. Please install Homebrew first:"
                echo "https://brew.sh"
                exit 1
            fi
            ;;
            
        "windows64")
            echo "Windows setup detected."
            echo "Please install the following manually:"
            echo "1. Visual Studio 2019/2022 with C++ support"
            echo "2. CMake (https://cmake.org/download/)"
            echo "3. Vulkan SDK (https://vulkan.lunarg.com/)"
            echo "4. Git for Windows (if not already installed)"
            echo ""
            echo "GLFW and other dependencies will be handled by vcpkg or manual download."
            echo "Consider using vcpkg for dependency management on Windows."
            ;;
    esac
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
                echo "Options:"
                echo "  --skip-deps    Skip system dependency installation"
                echo "  --skip-build   Skip building the project"
                echo "  --help, -h     Show this help message"
                exit 0
                ;;
        esac
    done
    
    # Run setup steps
    if [ "$SKIP_DEPS" = false ]; then
        install_system_deps
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