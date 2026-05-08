#pragma once

#include "cef_client_impl.h"
#include "include/wrapper/cef_message_router.h"

class CefFormsClient : public CefClientImpl {
public:
    CefFormsClient(CefRefPtr<CefRenderHandlerImpl> renderHandler);

    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        CefProcessId source_process,
                                        CefRefPtr<CefProcessMessage> message) override;

    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    void AddMessageHandler(CefMessageRouterBrowserSide::Handler* handler);

private:
    CefRefPtr<CefMessageRouterBrowserSide> m_MessageRouter;
    IMPLEMENT_REFCOUNTING(CefFormsClient);
};
