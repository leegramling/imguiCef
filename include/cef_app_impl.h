#pragma once

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"

class CefAppImpl : public CefApp, public CefBrowserProcessHandler {
public:
    CefAppImpl() = default;
    
    // CefApp methods
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    
    // CefApp methods
    virtual void OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) override;
    
    // CefBrowserProcessHandler methods
    virtual void OnContextInitialized() override;
    
private:
    IMPLEMENT_REFCOUNTING(CefAppImpl);
};