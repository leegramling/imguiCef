#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"

// Simple test app that doesn't require graphics
class TestApp : public CefApp {
public:
    TestApp() = default;

    // CefApp methods
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override {
        
        // Disable GPU and graphics-related features for headless testing
        command_line->AppendSwitch("--disable-gpu");
        command_line->AppendSwitch("--disable-gpu-compositing");
        command_line->AppendSwitch("--disable-gpu-sandbox");
        command_line->AppendSwitch("--disable-software-rasterizer");
        command_line->AppendSwitch("--headless");
        command_line->AppendSwitch("--no-sandbox");
        command_line->AppendSwitch("--disable-dev-shm-usage");
        command_line->AppendSwitch("--disable-extensions");
        command_line->AppendSwitch("--disable-plugins");
        command_line->AppendSwitch("--disable-web-security");
        command_line->AppendSwitch("--disable-features=VizDisplayCompositor");
    }

private:
    IMPLEMENT_REFCOUNTING(TestApp);
    DISALLOW_COPY_AND_ASSIGN(TestApp);
};

int main(int argc, char* argv[]) {
    std::cout << "Starting CEF initialization test..." << std::endl;

    // Provide CEF with command-line arguments
    CefMainArgs main_args(argc, argv);

    // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
    // This function checks the command-line and, if this is a sub-process, executes the appropriate logic
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        // The sub-process has completed so return here
        return exit_code;
    }

    // Create the CEF settings
    CefSettings settings;
    
    // Configure settings for headless operation
    settings.windowless_rendering_enabled = false;  // We don't need windowless rendering for this test
    settings.no_sandbox = true;                     // Disable sandbox for testing
    settings.log_severity = LOGSEVERITY_INFO;       // Enable logging for debugging
    
    // Set log file path (optional)
    CefString(&settings.log_file).FromASCII("cef_test.log");
    
    // Initialize CEF
    CefRefPtr<TestApp> app = new TestApp();
    
    std::cout << "Calling CefInitialize..." << std::endl;
    bool result = CefInitialize(main_args, settings, app.get(), nullptr);
    
    if (!result) {
        std::cerr << "ERROR: CefInitialize failed!" << std::endl;
        return 1;
    }
    
    std::cout << "CEF initialized successfully!" << std::endl;
    
    // Run a minimal message loop to ensure CEF is properly initialized
    // This is important because CEF initialization is asynchronous
    std::cout << "Running minimal message loop..." << std::endl;
    
    // Run for a short period to allow initialization to complete
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        CefDoMessageLoopWork();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Message loop completed." << std::endl;
    
    // Shutdown CEF
    std::cout << "Shutting down CEF..." << std::endl;
    CefShutdown();
    
    std::cout << "CEF initialization test completed successfully!" << std::endl;
    return 0;
}