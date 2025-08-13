#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"
#include "include/internal/cef_types.h"

#include "../include/vulkan_renderer.h"
#include "../include/cef_app_impl.h"
#include "../include/cef_client_impl.h"

class Application {
public:
    bool Initialize(int argc, char* argv[]);
    void Run();
    void Cleanup();

private:
    GLFWwindow* m_Window = nullptr;
    std::unique_ptr<VulkanRenderer> m_Renderer;
    CefRefPtr<CefAppImpl> m_CefApp;
    CefRefPtr<CefRenderHandlerImpl> m_RenderHandler;
    CefRefPtr<CefClientImpl> m_Client;
    CefRefPtr<CefBrowser> m_Browser;
    
    // Vulkan resources for CEF texture
    VkImage m_CefTextureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_CefTextureMemory = VK_NULL_HANDLE;
    VkImageView m_CefTextureView = VK_NULL_HANDLE;
    VkSampler m_CefTextureSampler = VK_NULL_HANDLE;
    VkDescriptorSet m_CefDescriptorSet = VK_NULL_HANDLE;
    
    int m_BrowserWidth = 800;
    int m_BrowserHeight = 600;
    char m_UrlBuffer[256] = "https://www.google.com";
    
    bool InitializeCEF(int argc, char* argv[]);
    bool InitializeWindow();
    bool InitializeVulkan();
    bool InitializeImGui();
    void CreateBrowser();
    void UpdateCefTexture();
    void RenderUI();
    void HandleInputEvents();
};

bool Application::Initialize(int argc, char* argv[]) {
    if (!InitializeCEF(argc, argv)) {
        std::cerr << "Failed to initialize CEF" << std::endl;
        return false;
    }
    
    if (!InitializeWindow()) {
        std::cerr << "Failed to initialize window" << std::endl;
        return false;
    }
    
    if (!InitializeVulkan()) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        return false;
    }
    
    if (!InitializeImGui()) {
        std::cerr << "Failed to initialize ImGui" << std::endl;
        return false;
    }
    
    CreateBrowser();
    
    return true;
}

bool Application::InitializeCEF(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    m_CefApp = new CefAppImpl();
    
    // Execute the sub-process if applicable
    int exit_code = CefExecuteProcess(main_args, m_CefApp, nullptr);
    if (exit_code >= 0) {
        exit(exit_code);
    }
    
    // Configure CEF settings
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;
    
    // Set cache directory to avoid singleton behavior warnings
    CefString(&settings.root_cache_path).FromASCII("./cef_cache");
    
    // Enable logging for debugging
    settings.log_severity = LOGSEVERITY_INFO;
    CefString(&settings.log_file).FromASCII("./debug.log");
    
    // Add command line switches to disable GPU acceleration
    settings.command_line_args_disabled = false;
    
#if !defined(OS_WIN)
    // On Linux, we need to set the resource paths - use current directory
    // which should be the build directory when running
    CefString(&settings.locales_dir_path).FromASCII("./locales");
    CefString(&settings.resources_dir_path).FromASCII(".");
    
    // Debug: Print current working directory and check for required files
    std::cout << "Current working directory should contain CEF resources" << std::endl;
    std::cout << "Looking for icudtl.dat, locales/, etc. in current directory" << std::endl;
#endif
    
    // Initialize CEF
    if (!CefInitialize(main_args, settings, m_CefApp, nullptr)) {
        return false;
    }
    
    return true;
}

bool Application::InitializeWindow() {
    if (!glfwInit()) {
        return false;
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(1280, 720, "ImGui + CEF + Vulkan Browser", nullptr, nullptr);
    
    return m_Window != nullptr;
}

bool Application::InitializeVulkan() {
    m_Renderer = std::make_unique<VulkanRenderer>();
    return m_Renderer->Initialize(m_Window);
}

bool Application::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForVulkan(m_Window, true);
    
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_Renderer->GetInstance();
    init_info.PhysicalDevice = m_Renderer->GetPhysicalDevice();
    init_info.Device = m_Renderer->GetDevice();
    init_info.QueueFamily = m_Renderer->GetQueueFamily();
    init_info.Queue = m_Renderer->GetGraphicsQueue();
    init_info.DescriptorPool = m_Renderer->GetDescriptorPool();
    init_info.RenderPass = m_Renderer->GetRenderPass();
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    
    ImGui_ImplVulkan_Init(&init_info);
    
    return true;
}

void Application::CreateBrowser() {
    // Create render handler and client
    m_RenderHandler = new CefRenderHandlerImpl(m_BrowserWidth, m_BrowserHeight);
    m_Client = new CefClientImpl(m_RenderHandler);
    
    // Configure browser window info
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);
    
    // Configure browser settings
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;
    
    // Create the browser
    CefBrowserHost::CreateBrowser(window_info, m_Client, m_UrlBuffer, browser_settings, nullptr, nullptr);
}

void Application::UpdateCefTexture() {
    if (!m_RenderHandler->IsDirty()) {
        return;
    }
    
    std::vector<uint8_t> textureData;
    int width, height;
    m_RenderHandler->GetTextureData(textureData, width, height);
    
    // Create or recreate texture if size changed
    if (m_CefTextureImage == VK_NULL_HANDLE || width != m_BrowserWidth || height != m_BrowserHeight) {
        m_BrowserWidth = width;
        m_BrowserHeight = height;
        
        // Clean up old resources
        if (m_CefTextureView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Renderer->GetDevice(), m_CefTextureView, nullptr);
        }
        if (m_CefTextureImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_Renderer->GetDevice(), m_CefTextureImage, nullptr);
            vkFreeMemory(m_Renderer->GetDevice(), m_CefTextureMemory, nullptr);
        }
        
        // Create new texture
        m_CefTextureImage = m_Renderer->CreateTextureImage(width, height, textureData.data());
        m_CefTextureView = m_Renderer->CreateImageView(m_CefTextureImage, VK_FORMAT_R8G8B8A8_UNORM);
        
        if (m_CefTextureSampler == VK_NULL_HANDLE) {
            m_CefTextureSampler = m_Renderer->CreateTextureSampler();
        }
        
        // Update descriptor set for ImGui
        m_CefDescriptorSet = ImGui_ImplVulkan_AddTexture(m_CefTextureSampler, m_CefTextureView, 
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        // Update existing texture
        m_Renderer->UpdateTextureImage(m_CefTextureImage, width, height, textureData.data());
    }
    
    m_RenderHandler->ClearDirty();
}

void Application::RenderUI() {
    // Single browser window with controls at the top
    ImGui::Begin("Browser", nullptr, ImGuiWindowFlags_NoCollapse);
    
    // URL controls at the top
    ImGui::Text("URL:");
    ImGui::SetNextItemWidth(-120); // Leave space for buttons
    ImGui::InputText("##url", m_UrlBuffer, sizeof(m_UrlBuffer));
    ImGui::SameLine();
    
    if (ImGui::Button("Go") && m_Client->GetBrowser()) {
        m_Client->GetBrowser()->GetMainFrame()->LoadURL(m_UrlBuffer);
    }
    
    // Navigation buttons on second row
    if (ImGui::Button("Back") && m_Client->GetBrowser()) {
        m_Client->GetBrowser()->GoBack();
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Forward") && m_Client->GetBrowser()) {
        m_Client->GetBrowser()->GoForward();
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Reload") && m_Client->GetBrowser()) {
        m_Client->GetBrowser()->Reload();
    }
    
    // Separator between controls and browser view
    ImGui::Separator();
    
    // Browser view below the controls
    if (m_CefDescriptorSet) {
        // Use fixed size for consistent layout
        ImVec2 browser_size = ImVec2((float)m_BrowserWidth, (float)m_BrowserHeight);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Display the browser image
        ImGui::Image((ImTextureID)m_CefDescriptorSet, browser_size);
        
        // Create an invisible button over the browser area to capture input
        ImGui::SetCursorScreenPos(pos);
        bool browser_focused = ImGui::InvisibleButton("browser_input", browser_size);
        
        // Handle input events when the browser area is active
        if (ImGui::IsItemHovered() && m_Client->GetBrowser()) {
            CefRefPtr<CefBrowserHost> host = m_Client->GetBrowser()->GetHost();
            ImGuiIO& io = ImGui::GetIO();
            
            // Get mouse position relative to browser area
            ImVec2 mouse_pos = ImGui::GetMousePos();
            int x = static_cast<int>(mouse_pos.x - pos.x);
            int y = static_cast<int>(mouse_pos.y - pos.y);
            
            // Create mouse event structure
            CefMouseEvent mouse_event;
            mouse_event.x = x;
            mouse_event.y = y;
            mouse_event.modifiers = 0;
            if (io.KeyCtrl) mouse_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
            if (io.KeyShift) mouse_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
            if (io.KeyAlt) mouse_event.modifiers |= EVENTFLAG_ALT_DOWN;
            
            // Send mouse movement
            host->SendMouseMoveEvent(mouse_event, false);
            
            // Handle mouse clicks
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                host->SendMouseClickEvent(mouse_event, MBT_LEFT, false, 1);
                host->SetFocus(true);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                host->SendMouseClickEvent(mouse_event, MBT_LEFT, true, 1);
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                host->SendMouseClickEvent(mouse_event, MBT_RIGHT, false, 1);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                host->SendMouseClickEvent(mouse_event, MBT_RIGHT, true, 1);
            }
            
            // Handle mouse wheel
            if (io.MouseWheel != 0.0f) {
                host->SendMouseWheelEvent(mouse_event, 0, static_cast<int>(io.MouseWheel * 120));
            }
            
            // Handle character input
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                ImWchar c = io.InputQueueCharacters[i];
                
                CefKeyEvent key_event;
                key_event.type = KEYEVENT_CHAR;
                key_event.character = c;
                key_event.unmodified_character = c;
                key_event.modifiers = mouse_event.modifiers;
                
                host->SendKeyEvent(key_event);
            }
        }
    } else {
        // Show placeholder when browser is not ready
        ImGui::Text("Browser loading...");
        ImGui::Dummy(ImVec2((float)m_BrowserWidth, (float)m_BrowserHeight));
    }
    
    ImGui::End();
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();
        
        // Process CEF events
        CefDoMessageLoopWork();
        
        // Update CEF texture
        UpdateCefTexture();
        
        // Begin frame
        m_Renderer->BeginFrame();
        
        // Start ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI
        RenderUI();
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_Renderer->GetCommandBuffer());
        
        // End frame
        m_Renderer->EndFrame();
    }
}

void Application::Cleanup() {
    // Wait for device to be idle
    if (m_Renderer) {
        vkDeviceWaitIdle(m_Renderer->GetDevice());
    }
    
    // Clean up Vulkan resources
    if (m_CefTextureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_Renderer->GetDevice(), m_CefTextureSampler, nullptr);
    }
    if (m_CefTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Renderer->GetDevice(), m_CefTextureView, nullptr);
    }
    if (m_CefTextureImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_Renderer->GetDevice(), m_CefTextureImage, nullptr);
        vkFreeMemory(m_Renderer->GetDevice(), m_CefTextureMemory, nullptr);
    }
    
    // Clean up ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Clean up renderer
    if (m_Renderer) {
        m_Renderer->Cleanup();
    }
    
    // Clean up window
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
    
    // Shut down CEF
    m_Client = nullptr;
    m_RenderHandler = nullptr;
    m_CefApp = nullptr;
    CefShutdown();
}

int main(int argc, char* argv[]) {
    Application app;
    
    if (!app.Initialize(argc, argv)) {
        return -1;
    }
    
    app.Run();
    app.Cleanup();
    
    return 0;
}