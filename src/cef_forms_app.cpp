#include "../include/cef_forms_app.h"

void CefFormsApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
    if (!m_MessageRouter) {
        CefMessageRouterConfig config;
        m_MessageRouter = CefMessageRouterRendererSide::Create(config);
    }
    m_MessageRouter->OnContextCreated(browser, frame, context);
}

void CefFormsApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {
    if (m_MessageRouter) {
        m_MessageRouter->OnContextReleased(browser, frame, context);
    }
}

bool CefFormsApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefProcessId source_process,
                                          CefRefPtr<CefProcessMessage> message) {
    if (m_MessageRouter) {
        return m_MessageRouter->OnProcessMessageReceived(browser, frame, source_process, message);
    }
    return false;
}
