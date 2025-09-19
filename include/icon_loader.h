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
        ~IconData() { if (pixels) delete[] pixels; }
    };

    static bool LoadAndSetWindowIcon(GLFWwindow* window, const std::string& iconPath = "icons");

private:
    static bool LoadPNGIcon(const std::string& filepath, IconData& icon);
    static std::vector<std::string> GetIconFilenames(const std::string& basePath);
};