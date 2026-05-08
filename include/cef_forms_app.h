#pragma once

#include "cef_app_impl.h"
#include "include/cef_render_process_handler.h"
#include "include/wrapper/cef_message_router.h"

class CefFormsApp : public CefAppImpl, public CefRenderProcessHandler {
public:
    CefFormsApp() = default;

    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    // CefRenderProcessHandler methods
    virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) override;
    virtual void OnContextReleased(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) override;
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        CefProcessId source_process,
                                        CefRefPtr<CefProcessMessage> message) override;

private:
    CefRefPtr<CefMessageRouterRendererSide> m_MessageRouter;
    IMPLEMENT_REFCOUNTING(CefFormsApp);
};
