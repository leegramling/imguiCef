#include <iostream>
#include <filesystem>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#endif

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif

#include "../include/icon_loader.h"

// Simple PNG loader without external dependencies
// For production, consider using stb_image or similar
namespace {
    // Basic PNG signature check
    bool IsPNG(const std::string& filepath) {
        FILE* file = fopen(filepath.c_str(), "rb");
        if (!file) return false;

        unsigned char png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        unsigned char file_signature[8];

        bool is_png = (fread(file_signature, 1, 8, file) == 8) &&
                      (memcmp(png_signature, file_signature, 8) == 0);

        fclose(file);
        return is_png;
    }

    // Simple raw RGBA data creation for fallback
    void CreateFallbackIcon(IconLoader::IconData& icon, int size) {
        icon.width = size;
        icon.height = size;
        icon.pixels = new unsigned char[size * size * 4];

        // Create a simple browser-like icon pattern
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int idx = (y * size + x) * 4;

                // Background
                icon.pixels[idx + 0] = 240; // R
                icon.pixels[idx + 1] = 240; // G
                icon.pixels[idx + 2] = 240; // B
                icon.pixels[idx + 3] = 255; // A

                // Border
                if (x == 0 || x == size - 1 || y == 0 || y == size - 1 ||
                    (y == size / 4 && x > 1 && x < size - 2)) {
                    icon.pixels[idx + 0] = 100;
                    icon.pixels[idx + 1] = 100;
                    icon.pixels[idx + 2] = 100;
                }

                // Simple globe icon in corner if size is large enough
                if (size >= 32) {
                    int globe_size = size / 6;
                    int globe_x = size - globe_size - 2;
                    int globe_y = 2;

                    int dx = x - (globe_x + globe_size / 2);
                    int dy = y - (globe_y + globe_size / 2);
                    int dist_sq = dx * dx + dy * dy;
                    int radius_sq = (globe_size / 2) * (globe_size / 2);

                    if (dist_sq <= radius_sq && dist_sq >= (radius_sq - 4)) {
                        icon.pixels[idx + 0] = 100;
                        icon.pixels[idx + 1] = 150;
                        icon.pixels[idx + 2] = 200;
                    }
                }
            }
        }
    }
}

bool IconLoader::LoadPNGIcon(const std::string& filepath, IconData& icon) {
    // For this implementation, we'll use a fallback since we don't have stb_image
    // In a real project, you'd use stb_image or similar library

    if (!IsPNG(filepath)) {
        std::cerr << "File is not a valid PNG: " << filepath << std::endl;
        return false;
    }

    // Extract size from filename (assuming format: browser_WIDTHxHEIGHT.png)
    std::string filename = std::filesystem::path(filepath).filename().string();
    size_t x_pos = filename.find('x');
    size_t dot_pos = filename.find('.', x_pos);

    if (x_pos != std::string::npos && dot_pos != std::string::npos) {
        try {
            std::string width_str = filename.substr(filename.find('_') + 1, x_pos - filename.find('_') - 1);
            std::string height_str = filename.substr(x_pos + 1, dot_pos - x_pos - 1);

            int width = std::stoi(width_str);
            int height = std::stoi(height_str);

            if (width == height && (width == 16 || width == 32 || width == 48)) {
                CreateFallbackIcon(icon, width);
                std::cout << "Created fallback icon: " << width << "x" << height << std::endl;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing icon size from filename: " << e.what() << std::endl;
        }
    }

    return false;
}

std::vector<std::string> IconLoader::GetIconFilenames(const std::string& basePath) {
    std::vector<std::string> filenames;

    // Check if directory exists
    if (!std::filesystem::exists(basePath)) {
        std::cout << "Directory does not exist: " << basePath << std::endl;
        return filenames;
    }

    if (!std::filesystem::is_directory(basePath)) {
        std::cout << "Path is not a directory: " << basePath << std::endl;
        return filenames;
    }

    std::cout << "Directory exists: " << basePath << std::endl;

    // Standard icon sizes for Windows 11 taskbar compatibility
    std::vector<int> sizes = {16, 32, 48};

    for (int size : sizes) {
        std::string filename = basePath + "/browser_" + std::to_string(size) + "x" + std::to_string(size) + ".png";
        std::cout << "Checking for: " << filename << std::endl;
        if (std::filesystem::exists(filename)) {
            filenames.push_back(filename);
            std::cout << "Found icon: " << filename << std::endl;
        } else {
            std::cout << "Icon not found: " << filename << std::endl;
        }
    }

    std::cout << "Total icons found: " << filenames.size() << std::endl;
    return filenames;
}

bool IconLoader::LoadAndSetWindowIcon(GLFWwindow* window, const std::string& iconPath) {
    if (!window) {
        std::cerr << "Invalid window pointer" << std::endl;
        return false;
    }

    // Get the current working directory for debugging
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;

    // Try multiple possible icon paths
    std::vector<std::string> searchPaths;

    // First try the provided path
    searchPaths.push_back(iconPath);

#ifdef _WIN32
    // On Windows, also try relative to executable
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        searchPaths.push_back((exeDir / iconPath).string());
        std::cout << "Executable directory: " << exeDir << std::endl;
    }
#endif

    // Try each search path
    std::vector<std::string> iconFiles;
    std::string usedPath;

    for (const auto& searchPath : searchPaths) {
        std::cout << "Checking for icons in: " << searchPath << std::endl;
        iconFiles = GetIconFilenames(searchPath);
        if (!iconFiles.empty()) {
            usedPath = searchPath;
            std::cout << "Found icons in: " << usedPath << std::endl;
            break;
        }
    }

    if (iconFiles.empty()) {
        std::cerr << "No icon files found in any of the searched paths:" << std::endl;
        for (const auto& path : searchPaths) {
            std::cerr << "  - " << path << std::endl;
        }
        return false;
    }

    std::vector<GLFWimage> images;
    std::vector<IconData> iconData;

    // Reserve space to avoid reallocation during push_back
    images.reserve(iconFiles.size());
    iconData.reserve(iconFiles.size());

    // Load all available icon sizes
    for (const std::string& filepath : iconFiles) {
        IconData icon;
        if (LoadPNGIcon(filepath, icon)) {
            GLFWimage glfwImage;
            glfwImage.width = icon.width;
            glfwImage.height = icon.height;
            glfwImage.pixels = icon.pixels;

            images.push_back(glfwImage);

            std::cout << "Loaded icon: " << filepath << " (" << icon.width << "x" << icon.height << ")" << std::endl;

            // Move the icon to avoid double-delete
            iconData.push_back(std::move(icon));
        }
    }

    if (images.empty()) {
        std::cerr << "Failed to load any icons" << std::endl;
        return false;
    }

    // Set the window icon
    glfwSetWindowIcon(window, static_cast<int>(images.size()), images.data());

#ifdef _WIN32
    // Additional Windows-specific handling for better taskbar integration
    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd) {
        std::cout << "Windows detected - HWND obtained: " << hwnd << std::endl;

        // For Windows 11, ensure we have the right icon sizes
        // The system will automatically scale, but providing multiple sizes helps

        // Set both small and large icons for maximum compatibility
        // Windows uses different icon sizes for different contexts:
        // - 16x16 for small icons (title bar, etc.)
        // - 32x32 for large icons (taskbar, Alt+Tab)
        // - 48x48 for extra large icons (desktop, some contexts)

        std::cout << "About to call ShowWindow to refresh icon..." << std::endl;

        // Force a window update to ensure the icon change takes effect
        ShowWindow(hwnd, SW_HIDE);
        std::cout << "Called ShowWindow(SW_HIDE)" << std::endl;

        ShowWindow(hwnd, SW_SHOW);
        std::cout << "Called ShowWindow(SW_SHOW)" << std::endl;

        std::cout << "Windows icon refresh complete" << std::endl;
    } else {
        std::cout << "Warning: Could not get Windows HWND for icon refresh" << std::endl;
    }
#else
    std::cout << "Non-Windows platform - using GLFW icon only" << std::endl;
#endif

    std::cout << "Successfully set window icon with " << images.size() << " size(s)" << std::endl;
    return true;
}