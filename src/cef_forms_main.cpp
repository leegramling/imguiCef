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
#include <mutex>
#include <queue>
#include <atomic>
#include <random>

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

// --- UTILS ---
std::filesystem::path GetExecutablePath() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer);
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::filesystem::path(std::string(result, (count > 0) ? count : 0));
#endif
}

#ifdef _WIN32
void SetCefPath(cef_string_t& target, const std::filesystem::path& path) {
    CefString(&target).FromASCII(path.string().c_str());
}
#endif

// --- CORE DATA STRUCTURES ---

enum class CommandType { CallDispatch, SkipDelivery };

struct Command {
    CommandType type;
    int driverId;
    bool boolVal;
};

struct DriverData {
    int id;
    std::string name;
    int ptd;
    int delivered;
    std::string status;
    std::string status_text;
    int eta;
    bool callDispatch;
    int stuck_ticks;
};

struct TodoData {
    int id;
    std::string text;
    bool completed;
};

// Thread-safe MPSC Queue for Commands
class MessageQueue {
public:
    void Push(Command cmd) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Queue.push(cmd);
    }
    bool Pop(Command& cmd) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Queue.empty()) return false;
        cmd = m_Queue.front();
        m_Queue.pop();
        return true;
    }
private:
    std::mutex m_Mutex;
    std::queue<Command> m_Queue;
};

// --- HANDLERS (Properly Refcounted) ---

class TodoHandler : public CefMessageRouterBrowserSide::Handler, public CefBaseRefCounted {
public:
    virtual bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, bool persistent, CefRefPtr<Callback> callback) override {
        CefRefPtr<CefValue> root = CefParseJSON(request, JSON_PARSER_RFC);
        if (!root || root->GetType() != VTYPE_DICTIONARY) return false;
        auto dict = root->GetDictionary();
        std::string action = dict->GetString("action").ToString();

        static std::vector<TodoData> todos;
        static int nextId = 1;

        if (action == "create") {
            auto data = dict->GetDictionary("data");
            todos.push_back({ nextId++, data->GetString("text").ToString(), data->GetBool("completed") });
            callback->Success("");
        } else if (action == "read") {
            CefRefPtr<CefListValue> list = CefListValue::Create();
            for (size_t i = 0; i < todos.size(); ++i) {
                CefRefPtr<CefDictionaryValue> td = CefDictionaryValue::Create();
                td->SetInt("id", todos[i].id);
                td->SetString("text", todos[i].text);
                td->SetBool("completed", todos[i].completed);
                list->SetDictionary(static_cast<int>(i), td);
            }
            CefRefPtr<CefValue> val = CefValue::Create(); val->SetList(list);
            callback->Success(CefWriteJSON(val, JSON_WRITER_DEFAULT));
        } else if (action == "update") {
            auto data = dict->GetDictionary("data");
            int id = data->GetInt("id");
            auto it = std::find_if(todos.begin(), todos.end(), [id](const TodoData& t) { return t.id == id; });
            if (it != todos.end() && data->HasKey("completed")) {
                it->completed = data->GetBool("completed");
                callback->Success("");
            } else callback->Failure(404, "Not found");
        } else if (action == "delete") {
            int id = dict->GetDictionary("data")->GetInt("id");
            todos.erase(std::remove_if(todos.begin(), todos.end(), [id](const TodoData& t) { return t.id == id; }), todos.end());
            callback->Success("");
        }
        return true;
    }
private:
    IMPLEMENT_REFCOUNTING(TodoHandler);
};

// --- SIMULATOR ACTOR ---

class DeliverySimulator {
public:
    DeliverySimulator() : m_Running(false), m_HasNewState(false) {
        m_Drivers = {
            { 1, "John Smith", 24, 12, "Green", "On Schedule", 45, false, 0 },
            { 2, "Sarah Connor", 30, 5, "Yellow", "Behind Schedule", 85, false, 0 },
            { 3, "Mike Ross", 18, 15, "Green", "On Schedule", 20, true, 0 },
            { 4, "Elena Fisher", 22, 8, "Green", "On Schedule", 55, false, 0 }
        };
    }

    void Start() {
        if (m_Running) return;
        m_Running = true;
        m_Thread = std::thread(&DeliverySimulator::WorkerLoop, this);
    }

    void Stop() {
        m_Running = false;
        if (m_Thread.joinable()) m_Thread.join();
    }

    void SendCommand(Command cmd) {
        m_Inbox.Push(cmd);
    }

    bool ConsumeState(std::string& state) {
        if (!m_HasNewState) return false;
        std::lock_guard<std::recursive_mutex> lock(m_StateMutex);
        state = m_LatestState;
        m_HasNewState = false;
        return true;
    }

    std::string GetCurrentStateJSON() {
        std::lock_guard<std::recursive_mutex> lock(m_StateMutex);
        CefRefPtr<CefListValue> list = CefListValue::Create();
        for (size_t i = 0; i < m_Drivers.size(); ++i) {
            CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
            d->SetInt("id", m_Drivers[i].id);
            d->SetString("name", m_Drivers[i].name);
            d->SetInt("ptd", m_Drivers[i].ptd);
            d->SetInt("delivered", m_Drivers[i].delivered);
            d->SetString("status", m_Drivers[i].status);
            d->SetString("status_text", m_Drivers[i].status_text);
            d->SetInt("eta", m_Drivers[i].eta);
            d->SetBool("callDispatch", m_Drivers[i].callDispatch);
            list->SetDictionary(static_cast<int>(i), d);
        }
        CefRefPtr<CefValue> val = CefValue::Create(); val->SetList(list);
        return CefWriteJSON(val, JSON_WRITER_DEFAULT).ToString();
    }

private:
    void WorkerLoop() {
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, 29);

        while (m_Running) {
            Command cmd;
            while (m_Inbox.Pop(cmd)) {
                auto it = std::find_if(m_Drivers.begin(), m_Drivers.end(), [cmd](const DriverData& d) { return d.id == cmd.driverId; });
                if (it != m_Drivers.end()) {
                    if (cmd.type == CommandType::CallDispatch) it->callDispatch = cmd.boolVal;
                    else if (cmd.type == CommandType::SkipDelivery && it->ptd > 0) it->ptd--;
                }
            }

            for (auto& d : m_Drivers) {
                if (d.stuck_ticks > 0) {
                    if (--d.stuck_ticks == 0) { d.status = "Green"; d.status_text = "On Schedule"; }
                    continue;
                }
                if (d.eta > 0) d.eta--;
                if (d.ptd > 0 && (distribution(generator) % 5 == 0)) { d.ptd--; d.delivered++; }
                
                int chance = distribution(generator);
                if (chance == 0) { d.status = "Red"; d.status_text = "Accident"; d.stuck_ticks = 10; }
                else if (chance == 1) { d.status = "Blue"; d.status_text = "Customer Incident"; d.stuck_ticks = 5; }
                else if (d.eta < 10 && d.eta > 0) { d.status = "Yellow"; d.status_text = "Behind Schedule"; }
            }

            {
                std::lock_guard<std::recursive_mutex> lock(m_StateMutex);
                m_LatestState = GetCurrentStateJSON();
                m_HasNewState = true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::vector<DriverData> m_Drivers;
    MessageQueue m_Inbox;
    std::thread m_Thread;
    std::atomic<bool> m_Running;
    
    std::recursive_mutex m_StateMutex;
    std::string m_LatestState;
    std::atomic<bool> m_HasNewState;
};

// --- CEF BRIDGES ---

class DeliveryBridge : public CefMessageRouterBrowserSide::Handler, public CefBaseRefCounted {
public:
    DeliveryBridge(DeliverySimulator* sim) : m_Sim(sim) {}
    virtual bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, bool persistent, CefRefPtr<Callback> callback) override {
        CefRefPtr<CefValue> root = CefParseJSON(request, JSON_PARSER_RFC);
        if (!root || root->GetType() != VTYPE_DICTIONARY) return false;
        auto dict = root->GetDictionary();
        std::string action = dict->GetString("action").ToString();

        if (action == "get_initial") {
            callback->Success(m_Sim->GetCurrentStateJSON());
        } else if (action == "call_dispatch") {
            auto data = dict->GetDictionary("data");
            m_Sim->SendCommand({ CommandType::CallDispatch, data->GetInt("id"), data->GetBool("value") });
            callback->Success("");
        } else if (action == "skip_delivery") {
            auto data = dict->GetDictionary("data");
            m_Sim->SendCommand({ CommandType::SkipDelivery, data->GetInt("id"), false });
            callback->Success("");
        }
        return true;
    }
private:
    DeliverySimulator* m_Sim;
    IMPLEMENT_REFCOUNTING(DeliveryBridge);
};

// --- UI INFRASTRUCTURE ---

struct BrowserInstance {
    CefRefPtr<CefFormsClient> client;
    CefRefPtr<CefRenderHandlerImpl> renderHandler;
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory = VK_NULL_HANDLE;
    VkImageView textureView = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    int width = 800, height = 600;
    std::string name;

    void UpdateTexture(VulkanRenderer* renderer, VkSampler sampler) {
        if (!renderer || !renderHandler || !renderHandler->IsDirty()) return;
        std::vector<uint8_t> data; int w, h;
        renderHandler->GetTextureData(data, w, h);
        if (w <= 0 || h <= 0 || data.empty()) return;

        if (textureImage == VK_NULL_HANDLE || w != width || h != height) {
            width = w; height = h;
            if (textureView != VK_NULL_HANDLE) vkDestroyImageView(renderer->GetDevice(), textureView, nullptr);
            if (textureImage != VK_NULL_HANDLE) { vkDestroyImage(renderer->GetDevice(), textureImage, nullptr); vkFreeMemory(renderer->GetDevice(), textureMemory, nullptr); }
            textureImage = renderer->CreateTextureImage(width, height, data.data(), textureMemory);
            if (textureImage == VK_NULL_HANDLE) return;
            textureView = renderer->CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
            descriptorSet = ImGui_ImplVulkan_AddTexture(sampler, textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else renderer->UpdateTextureImage(textureImage, width, height, data.data());
        renderHandler->ClearDirty();
    }

    void Cleanup(VkDevice device) {
        if (device == VK_NULL_HANDLE) return;
        if (textureView != VK_NULL_HANDLE) vkDestroyImageView(device, textureView, nullptr);
        if (textureImage != VK_NULL_HANDLE) { vkDestroyImage(device, textureImage, nullptr); vkFreeMemory(device, textureMemory, nullptr); }
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
    
    BrowserInstance m_DeliveryDashboard;
    BrowserInstance m_TodoApp;
    DeliverySimulator m_Simulator;

    bool m_ShowDelivery = true;
    bool m_ShowTodo = false;

    bool InitializeCEF(int argc, char* argv[]);
    void CreateBrowser(BrowserInstance& instance, const std::string& url, CefMessageRouterBrowserSide::Handler* handler);
    void RenderBrowserWindow(BrowserInstance& instance, bool* p_open, const std::string& url, CefMessageRouterBrowserSide::Handler* handler);
};

bool Application::Initialize(int argc, char* argv[]) {
    if (!InitializeCEF(argc, argv)) return false;
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(1400, 900, "cefForms Multi-UI", nullptr, nullptr);
    if (!m_Window) return false;
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
    m_DeliveryDashboard.name = "Delivery Dashboard";
    m_TodoApp.name = "ToDo Application";
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
    
    CefSettings s; s.windowless_rendering_enabled = true; s.no_sandbox = true;
    auto exe_dir = GetExecutablePath().parent_path();
#ifdef _WIN32
    SetCefPath(s.root_cache_path, exe_dir / "cef_cache");
    SetCefPath(s.resources_dir_path, exe_dir / "cef");
    SetCefPath(s.locales_dir_path, exe_dir / "cef" / "locales");
#else
    CefString(&s.root_cache_path).FromASCII((exe_dir / "cef_cache").string().c_str());
    CefString(&s.locales_dir_path).FromASCII((exe_dir / "locales").string().c_str());
    CefString(&s.resources_dir_path).FromASCII(exe_dir.string().c_str());
#endif
    return CefInitialize(args, s, m_CefApp, nullptr);
}

void Application::CreateBrowser(BrowserInstance& inst, const std::string& url, CefMessageRouterBrowserSide::Handler* handler) {
    inst.renderHandler = new CefRenderHandlerImpl(inst.width, inst.height);
    inst.client = new CefFormsClient(inst.renderHandler);
    if (handler) inst.client->AddMessageHandler(handler);
    CefWindowInfo win; win.SetAsWindowless(0);
    CefBrowserSettings bs; bs.windowless_frame_rate = 60;
    CefBrowserHost::CreateBrowser(win, inst.client, url, bs, nullptr, nullptr);
}

void Application::RenderBrowserWindow(BrowserInstance& inst, bool* p_open, const std::string& url, CefMessageRouterBrowserSide::Handler* handler) {
    if (!*p_open) return;
    if (!inst.client) CreateBrowser(inst, url, handler);
    ImGui::SetNextWindowSize(ImVec2((float)inst.width + 20, (float)inst.height + 40), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(inst.name.c_str(), p_open)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        int aw = std::max(64, (int)avail.x), ah = std::max(64, (int)avail.y);
        auto browser = inst.client->GetBrowser();
        if (browser && browser->GetHost() && (aw != inst.width || ah != inst.height)) {
            inst.width = aw; inst.height = ah;
            inst.renderHandler->Resize(aw, ah);
            browser->GetHost()->WasResized();
        }
        if (inst.descriptorSet) {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)inst.descriptorSet, ImVec2((float)inst.width, (float)inst.height));
            ImGui::SetCursorScreenPos(cp);
            ImGui::InvisibleButton((inst.name + "_btn").c_str(), ImVec2((float)inst.width, (float)inst.height));
            if (ImGui::IsItemHovered() && browser && browser->GetHost()) {
                auto h = browser->GetHost();
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
        
        std::string latestState;
        if (m_Simulator.ConsumeState(latestState)) {
            if (m_DeliveryDashboard.client && m_DeliveryDashboard.client->GetBrowser()) {
                auto frame = m_DeliveryDashboard.client->GetBrowser()->GetMainFrame();
                if (frame) {
                    std::string js = "if(window.updateDrivers) { window.updateDrivers(" + latestState + "); }";
                    frame->ExecuteJavaScript(js, frame->GetURL(), 0);
                }
            }
        }

        if (m_Renderer) {
            m_DeliveryDashboard.UpdateTexture(m_Renderer.get(), m_CefTextureSampler);
            m_TodoApp.UpdateTexture(m_Renderer.get(), m_CefTextureSampler);
        }
        
        m_Renderer->BeginFrame();
        ImGui_ImplVulkan_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Quit", "Alt+F4")) glfwSetWindowShouldClose(m_Window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Delivery Dashboard", nullptr, &m_ShowDelivery);
                ImGui::MenuItem("ToDo Application", nullptr, &m_ShowTodo);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (m_ShowDelivery) {
            if (!m_DeliveryDashboard.client) {
                CreateBrowser(m_DeliveryDashboard, base_url + "delivery.html", new DeliveryBridge(&m_Simulator));
                m_Simulator.Start();
            }
            RenderBrowserWindow(m_DeliveryDashboard, &m_ShowDelivery, base_url + "delivery.html", nullptr);
        }
        if (m_ShowTodo) {
            RenderBrowserWindow(m_TodoApp, &m_ShowTodo, base_url + "todo.html", new TodoHandler());
        }
        
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_Renderer->GetCommandBuffer());
        m_Renderer->EndFrame();
    }
}

void Application::Cleanup() {
    m_Simulator.Stop();
    if (m_Renderer) {
        vkDeviceWaitIdle(m_Renderer->GetDevice());
        if (m_CefTextureSampler != VK_NULL_HANDLE) vkDestroySampler(m_Renderer->GetDevice(), m_CefTextureSampler, nullptr);
        m_DeliveryDashboard.Cleanup(m_Renderer->GetDevice());
        m_TodoApp.Cleanup(m_Renderer->GetDevice());
        ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
        m_Renderer->Cleanup(); 
    }
    if (m_Window) { glfwDestroyWindow(m_Window); glfwTerminate(); }
    m_CefApp = nullptr; CefShutdown();
}

int main(int argc, char* argv[]) {
    Application app; 
    if (!app.Initialize(argc, argv)) {
        app.Cleanup();
        return -1;
    }
    app.Run(); 
    app.Cleanup(); 
    return 0;
}
