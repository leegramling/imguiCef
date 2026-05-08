#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"
#include "include/internal/cef_types.h"

#include "../include/vulkan_renderer.h"
#include "../include/cef_app_impl.h"
#include "../include/cef_client_impl.h"
#include "../include/cef_forms_app.h"
#include "../include/cef_forms_client.h"

#ifdef _WIN32
namespace {
std::filesystem::path GetExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

void SetCefPath(cef_string_t& target, const std::filesystem::path& path) {
    CefString(&target).FromASCII(path.string().c_str());
}
}  // namespace
#endif

// --- TODO APP DATA ---
struct TodoData {
    int id;
    std::string text;
    bool completed;
};
std::vector<TodoData> g_Todos;
int g_NextId = 1;

class TodoHandler : public CefMessageRouterBrowserSide::Handler {
public:
    virtual bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, bool persistent, CefRefPtr<Callback> callback) override {
        CefRefPtr<CefValue> root = CefParseJSON(request, JSON_PARSER_RFC);
        if (!root || root->GetType() != VTYPE_DICTIONARY) return false;
        CefRefPtr<CefDictionaryValue> dict = root->GetDictionary();
        std::string action = dict->GetString("action").ToString();
        if (action == "create") {
            CefRefPtr<CefDictionaryValue> data = dict->GetDictionary("data");
            TodoData todo = { g_NextId++, data->GetString("text").ToString(), data->GetBool("completed") };
            g_Todos.push_back(todo);
            callback->Success("");
        } else if (action == "read") {
            CefRefPtr<CefListValue> list = CefListValue::Create();
            for (size_t i = 0; i < g_Todos.size(); ++i) {
                CefRefPtr<CefDictionaryValue> td = CefDictionaryValue::Create();
                td->SetInt("id", g_Todos[i].id);
                td->SetString("text", g_Todos[i].text);
                td->SetBool("completed", g_Todos[i].completed);
                list->SetDictionary(static_cast<int>(i), td);
            }
            CefRefPtr<CefValue> val = CefValue::Create(); val->SetList(list);
            callback->Success(CefWriteJSON(val, JSON_WRITER_DEFAULT));
        } else if (action == "update") {
            CefRefPtr<CefDictionaryValue> data = dict->GetDictionary("data");
            int id = data->GetInt("id");
            auto it = std::find_if(g_Todos.begin(), g_Todos.end(), [id](const TodoData& t) { return t.id == id; });
            if (it != g_Todos.end()) {
                if (data->HasKey("completed")) it->completed = data->GetBool("completed");
                callback->Success("");
            } else callback->Failure(404, "Not found");
        } else if (action == "delete") {
            int id = dict->GetDictionary("data")->GetInt("id");
            g_Todos.erase(std::remove_if(g_Todos.begin(), g_Todos.end(), [id](const TodoData& t) { return t.id == id; }), g_Todos.end());
            callback->Success("");
        }
        return true;
    }
};

// --- PERF MONITOR DATA ---
class PerfHandler : public CefMessageRouterBrowserSide::Handler {
public:
    virtual bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, bool persistent, CefRefPtr<Callback> callback) override {
        CefRefPtr<CefValue> root = CefParseJSON(request, JSON_PARSER_RFC);
        if (!root || root->GetType() != VTYPE_DICTIONARY) return false;
        if (root->GetDictionary()->GetString("action") != "get_stats") return false;

        CefRefPtr<CefDictionaryValue> stats = CefDictionaryValue::Create();
        stats->SetInt("cpu_total", 15 + (rand() % 20));
        stats->SetDouble("mem_used_gb", 4.2 + (rand() % 10) / 10.0);
        stats->SetInt("mem_percent", 42);
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss; ss << std::put_time(std::localtime(&in_time_t), "%X");
        stats->SetString("uptime", ss.str());

        CefRefPtr<CefListValue> procs = CefListValue::Create();
        const char* names[] = {"cefForms.exe", "Chromium", "ImGui Layer", "Vulkan Driver", "System"};
        for(int i=0; i<5; ++i) {
            CefRefPtr<CefDictionaryValue> p = CefDictionaryValue::Create();
            p->SetString("name", names[i]);
            p->SetInt("cpu", 1 + (rand() % 10));
            p->SetInt("mem", 50 + (rand() % 200));
            procs->SetDictionary(i, p);
        }
        stats->SetList("processes", procs);

        CefRefPtr<CefValue> val = CefValue::Create(); val->SetDictionary(stats);
        callback->Success(CefWriteJSON(val, JSON_WRITER_DEFAULT));
        return true;
    }
};

// --- CEF BROWSER INSTANCE HELPER ---
struct BrowserInstance {
    CefRefPtr<CefFormsClient> client;
    CefRefPtr<CefRenderHandlerImpl> renderHandler;
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory = VK_NULL_HANDLE;
    VkImageView textureView = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    int width = 600;
    int height = 700;
    std::string name;

    void UpdateTexture(VulkanRenderer* renderer, VkSampler sampler) {
        if (!renderHandler || !renderHandler->IsDirty()) return;
        std::vector<uint8_t> data; int w, h;
        renderHandler->GetTextureData(data, w, h);
        if (textureImage == VK_NULL_HANDLE || w != width || h != height) {
            width = w; height = h;
            if (textureView) vkDestroyImageView(renderer->GetDevice(), textureView, nullptr);
            if (textureImage) { vkDestroyImage(renderer->GetDevice(), textureImage, nullptr); vkFreeMemory(renderer->GetDevice(), textureMemory, nullptr); }
            textureImage = renderer->CreateTextureImage(width, height, data.data(), textureMemory);
            textureView = renderer->CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
            descriptorSet = ImGui_ImplVulkan_AddTexture(sampler, textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else renderer->UpdateTextureImage(textureImage, width, height, data.data());
        renderHandler->ClearDirty();
    }

    void Cleanup(VkDevice device) {
        if (textureView) vkDestroyImageView(device, textureView, nullptr);
        if (textureImage) { vkDestroyImage(device, textureImage, nullptr); vkFreeMemory(device, textureMemory, nullptr); }
        client = nullptr; renderHandler = nullptr;
    }
};

class Application {
public:
    bool Initialize(int argc, char* argv[]);
    void Run();
    void Cleanup();

private:
    GLFWwindow* m_Window = nullptr;
    std::unique_ptr<VulkanRenderer> m_Renderer;
    CefRefPtr<CefFormsApp> m_CefApp;
    VkSampler m_CefTextureSampler = VK_NULL_HANDLE;
    
    BrowserInstance m_TodoApp;
    BrowserInstance m_PerfMon;

    bool m_ShowTodo = false;
    bool m_ShowPerf = true;

    bool InitializeCEF(int argc, char* argv[]);
    void CreateBrowser(BrowserInstance& instance, const std::string& url, CefMessageRouterBrowserSide::Handler* handler);
    void RenderBrowserWindow(BrowserInstance& instance, bool* p_open, const std::string& url, CefMessageRouterBrowserSide::Handler* handler);
};

bool Application::Initialize(int argc, char* argv[]) {
    if (!InitializeCEF(argc, argv)) return false;
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(1400, 900, "cefForms Multi-UI", nullptr, nullptr);
    m_Renderer = std::make_unique<VulkanRenderer>();
    if (!m_Renderer->Initialize(m_Window)) return false;

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(m_Window, true);
    ImGui_ImplVulkan_InitInfo ii = {};
    ii.Instance = m_Renderer->GetInstance(); ii.PhysicalDevice = m_Renderer->GetPhysicalDevice();
    ii.Device = m_Renderer->GetDevice(); ii.QueueFamily = m_Renderer->GetQueueFamily();
    ii.Queue = m_Renderer->GetGraphicsQueue(); ii.DescriptorPool = m_Renderer->GetDescriptorPool();
    ii.RenderPass = m_Renderer->GetRenderPass(); ii.MinImageCount = 2; ii.ImageCount = 2;
    ImGui_ImplVulkan_Init(&ii);

    m_CefTextureSampler = m_Renderer->CreateTextureSampler();

    std::string base_url = "file://";
#ifndef _WIN32
    char buf[PATH_MAX]; getcwd(buf, sizeof(buf)); base_url += std::string(buf) + "/assets/";
#else
    base_url += GetExecutablePath().parent_path().string() + "/assets/";
    std::replace(base_url.begin(), base_url.end(), '\\', '/');
#endif

    m_TodoApp.name = "TODO Application";
    // Browsers will be created on first open in RenderBrowserWindow
    
    m_PerfMon.name = "System Monitor";
    m_PerfMon.width = 500; m_PerfMon.height = 600;
    
    return true;
}

bool Application::InitializeCEF(int argc, char* argv[]) {
#ifdef _WIN32
    CefMainArgs args(GetModuleHandle(nullptr));
#else
    CefMainArgs args(argc, argv);
#endif
    m_CefApp = new CefFormsApp();
    int ec = CefExecuteProcess(args, m_CefApp, nullptr);
    if (ec >= 0) exit(ec);
    
    CefSettings s; 
    s.windowless_rendering_enabled = true; 
    s.no_sandbox = true;

    auto root_dir = std::filesystem::current_path();

#ifdef _WIN32
    auto exe_dir = GetExecutablePath().parent_path();
    SetCefPath(s.root_cache_path, exe_dir / "cef_cache");
    SetCefPath(s.resources_dir_path, exe_dir / "cef");
    SetCefPath(s.locales_dir_path, exe_dir / "cef" / "locales");
#else
    CefString(&s.root_cache_path).FromASCII(std::filesystem::absolute(root_dir / "cef_cache").string().c_str());
    CefString(&s.locales_dir_path).FromASCII(std::filesystem::absolute(root_dir / "locales").string().c_str());
    CefString(&s.resources_dir_path).FromASCII(std::filesystem::absolute(root_dir).string().c_str());
#endif
    return CefInitialize(args, s, m_CefApp, nullptr);
}

void Application::CreateBrowser(BrowserInstance& inst, const std::string& url, CefMessageRouterBrowserSide::Handler* handler) {
    inst.renderHandler = new CefRenderHandlerImpl(inst.width, inst.height);
    inst.client = new CefFormsClient(inst.renderHandler);
    inst.client->AddMessageHandler(handler);
    CefWindowInfo win; win.SetAsWindowless(0);
    CefBrowserSettings bs; bs.windowless_frame_rate = 60;
    CefBrowserHost::CreateBrowser(win, inst.client, url, bs, nullptr, nullptr);
}

void Application::RenderBrowserWindow(BrowserInstance& inst, bool* p_open, const std::string& url, CefMessageRouterBrowserSide::Handler* handler) {
    if (!*p_open) return;

    // Lazy initialization
    if (!inst.client) {
        CreateBrowser(inst, url, handler);
    }

    ImGui::SetNextWindowSize(ImVec2((float)inst.width + 20, (float)inst.height + 40), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(inst.name.c_str(), p_open)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        int aw = std::max(64, (int)avail.x), ah = std::max(64, (int)avail.y);
        if (inst.client->GetBrowser() && inst.client->GetBrowser()->GetHost() && (aw != inst.width || ah != inst.height)) {
            inst.width = aw; inst.height = ah;
            inst.renderHandler->Resize(aw, ah);
            inst.client->GetBrowser()->GetHost()->WasResized();
        }
        if (inst.descriptorSet) {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)inst.descriptorSet, ImVec2((float)inst.width, (float)inst.height));
            ImGui::SetCursorScreenPos(cp);
            ImGui::InvisibleButton((inst.name + "_btn").c_str(), ImVec2((float)inst.width, (float)inst.height));
            if (ImGui::IsItemHovered() && inst.client->GetBrowser() && inst.client->GetBrowser()->GetHost()) {
                auto h = inst.client->GetBrowser()->GetHost();
                ImGuiIO& io = ImGui::GetIO(); ImVec2 m = ImGui::GetMousePos();
                CefMouseEvent me; me.x = (int)(m.x - cp.x); me.y = (int)(m.y - cp.y); me.modifiers = 0;
                if (io.KeyCtrl) me.modifiers |= EVENTFLAG_CONTROL_DOWN;
                if (io.KeyShift) me.modifiers |= EVENTFLAG_SHIFT_DOWN;
                h->SendMouseMoveEvent(me, false);
                if (ImGui::IsMouseClicked(0)) { h->SendMouseClickEvent(me, MBT_LEFT, false, 1); h->SetFocus(true); }
                if (ImGui::IsMouseReleased(0)) h->SendMouseClickEvent(me, MBT_LEFT, true, 1);
                if (io.MouseWheel != 0.0f) h->SendMouseWheelEvent(me, 0, (int)(io.MouseWheel * 120));
                for (int i=0; i<io.InputQueueCharacters.Size; ++i) {
                    CefKeyEvent ke; ke.type = KEYEVENT_CHAR; ke.character = io.InputQueueCharacters[i]; h->SendKeyEvent(ke);
                }
            }
        }
    }
    ImGui::End();
}

void Application::Run() {
    std::string base_url = "file://";
#ifndef _WIN32
    char buf[PATH_MAX]; getcwd(buf, sizeof(buf)); base_url += std::string(buf) + "/assets/";
#else
    base_url += GetExecutablePath().parent_path().string() + "/assets/";
    std::replace(base_url.begin(), base_url.end(), '\\', '/');
#endif

    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();
        CefDoMessageLoopWork();
        
        m_TodoApp.UpdateTexture(m_Renderer.get(), m_CefTextureSampler);
        m_PerfMon.UpdateTexture(m_Renderer.get(), m_CefTextureSampler);
        
        m_Renderer->BeginFrame();
        ImGui_ImplVulkan_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Quit", "Alt+F4")) {
                    glfwSetWindowShouldClose(m_Window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("ToDo Application", nullptr, &m_ShowTodo);
                ImGui::MenuItem("System Monitor", nullptr, &m_ShowPerf);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (m_ShowTodo) {
            RenderBrowserWindow(m_TodoApp, &m_ShowTodo, base_url + "todo.html", new TodoHandler());
        }
        if (m_ShowPerf) {
            RenderBrowserWindow(m_PerfMon, &m_ShowPerf, base_url + "perf.html", new PerfHandler());
        }
        
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_Renderer->GetCommandBuffer());
        m_Renderer->EndFrame();
    }
}

void Application::Cleanup() {
    if (m_Renderer) {
        vkDeviceWaitIdle(m_Renderer->GetDevice());
        if (m_CefTextureSampler) vkDestroySampler(m_Renderer->GetDevice(), m_CefTextureSampler, nullptr);
        m_TodoApp.Cleanup(m_Renderer->GetDevice()); m_PerfMon.Cleanup(m_Renderer->GetDevice());
        ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
        m_Renderer->Cleanup(); 
    }
    if (m_Window) {
        glfwDestroyWindow(m_Window); glfwTerminate();
    }
    m_CefApp = nullptr; CefShutdown();
}

int main(int argc, char* argv[]) {
    Application app; if (!app.Initialize(argc, argv)) return -1;
    app.Run(); app.Cleanup(); return 0;
}
