#include "../include/cef_forms_client.h"

CefFormsClient::CefFormsClient(CefRefPtr<CefRenderHandlerImpl> renderHandler)
    : CefClientImpl(renderHandler) {
    CefMessageRouterConfig config;
    m_MessageRouter = CefMessageRouterBrowserSide::Create(config);
}

bool CefFormsClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefProcessId source_process,
                                            CefRefPtr<CefProcessMessage> message) {
    if (m_MessageRouter->OnProcessMessageReceived(browser, frame, source_process, message)) {
        return true;
    }
    return CefClientImpl::OnProcessMessageReceived(browser, frame, source_process, message);
}

void CefFormsClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    m_MessageRouter->OnBeforeClose(browser);
    CefClientImpl::OnBeforeClose(browser);
}

void CefFormsClient::AddMessageHandler(CefMessageRouterBrowserSide::Handler* handler) {
    m_MessageRouter->AddHandler(handler, false);
}
