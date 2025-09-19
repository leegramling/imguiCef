#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <string>

class IconLoader {
public:
    struct IconData {
        int width;
        int height;
        unsigned char* pixels;

        IconData() : width(0), height(0), pixels(nullptr) {}
        ~IconData() {
            if (pixels) {
                delete[] pixels;
                pixels = nullptr;
            }
        }

        // Prevent copying to avoid double-delete
        IconData(const IconData&) = delete;
        IconData& operator=(const IconData&) = delete;

        // Allow moving
        IconData(IconData&& other) noexcept
            : width(other.width), height(other.height), pixels(other.pixels) {
            other.pixels = nullptr;
            other.width = 0;
            other.height = 0;
        }

        IconData& operator=(IconData&& other) noexcept {
            if (this != &other) {
                if (pixels) delete[] pixels;
                width = other.width;
                height = other.height;
                pixels = other.pixels;
                other.pixels = nullptr;
                other.width = 0;
                other.height = 0;
            }
            return *this;
        }
    };

    static bool LoadAndSetWindowIcon(GLFWwindow* window, const std::string& iconPath = "icons");

private:
    static bool LoadPNGIcon(const std::string& filepath, IconData& icon);
    static std::vector<std::string> GetIconFilenames(const std::string& basePath);
};