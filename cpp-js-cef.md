# C++ and JavaScript Communication in CEF (cefForms)

This document explains how to perform bidirectional communication between C++ and JavaScript in the `cefForms` application using `CefMessageRouter`. This approach allows us to build modern UIs with HTML/CSS/JS while maintaining high-performance data processing and persistence in C++, without the need for an embedded web server.

## Overview

The `CefMessageRouter` facilitates asynchronous communication between the browser process (C++) and the renderer process (JavaScript). 

### Key Components

1.  **JavaScript Side:** Uses a global function (default: `window.cefQuery`) to send requests to C++.
2.  **C++ Browser Process:** Implements `CefMessageRouterBrowserSide` to receive and handle these requests.
3.  **C++ Renderer Process:** Implements `CefMessageRouterRendererSide` to hook into the JavaScript environment.

---

## 1. JavaScript API (The Frontend)

In JavaScript, you initiate a request to C++ using `window.cefQuery`.

### Example: Fetching Todos (Read)

```javascript
function fetchTodos() {
    window.cefQuery({
        request: JSON.stringify({ action: 'read' }),
        onSuccess: function(response) {
            const todos = JSON.parse(response);
            renderTodos(todos);
        },
        onFailure: function(errorCode, errorMessage) {
            console.error('Error fetching todos:', errorMessage);
        }
    });
}
```

### Example: Adding a Todo (Create)

```javascript
function addTodo(text) {
    window.cefQuery({
        request: JSON.stringify({ 
            action: 'create', 
            data: { text: text, completed: false } 
        }),
        onSuccess: function(response) {
            fetchTodos(); // Refresh list
        }
    });
}
```

---

## 2. C++ Backend (The Browser Process)

The C++ side is responsible for processing the requests, interacting with the data store, and sending back responses.

### Data Model

We use a simple struct to represent a Todo item:

```cpp
struct TodoData {
    int id;
    std::string text;
    bool completed;
};

std::vector<TodoData> g_Todos;
int g_NextId = 1;
```

### Request Handler

We implement `CefMessageRouterBrowserSide::Handler` to process the JSON requests.

```cpp
class TodoHandler : public CefMessageRouterBrowserSide::Handler {
public:
    virtual bool OnQuery(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64_t query_id,
                         const CefString& request,
                         bool persistent,
                         CefRefPtr<Callback> callback) override {
        
        // 1. Parse JSON request
        std::string json_request = request.ToString();
        // (Use CefJSONParser or a library like nlohmann/json)
        
        // 2. Perform CRUD based on "action"
        if (action == "read") {
            // Serialize g_Todos to JSON and return
            callback->Success(json_response);
        } else if (action == "create") {
            // Add to g_Todos
            callback->Success("");
        }
        
        return true; // Request handled
    }
};
```

---

## 3. Wiring it all together

### In CefClientImpl (Browser Process)

1.  Create `CefMessageRouterBrowserSide`.
2.  Add the `TodoHandler`.
3.  Forward `OnProcessMessageReceived` to the router.

### In CefAppImpl (Renderer Process)

1.  Create `CefMessageRouterRendererSide`.
2.  Forward `OnContextCreated` and `OnContextReleased` to the router.

---

## Handling Multiple CEF Windows

In `cefForms`, we can host multiple independent browsers (e.g., a Todo App and a System Monitor) within the same application. 

### Threading Architecture
-   **UI Thread (C++):** All `CefBrowser` instances are managed on the same C++ UI thread. When we call `CefDoMessageLoopWork()`, it processes events for *all* active browsers.
-   **Renderer Processes:** Each browser instance (or more accurately, each origin) typically runs in its own separate **Renderer Process**. This means a crash or heavy calculation in the Todo App's JavaScript won't freeze the System Monitor's UI.
-   **Thread Safety:** Since the `OnPaint` and `OnQuery` callbacks for multiple browsers can happen concurrently or in rapid succession, using a `std::mutex` in the `RenderHandler` and thread-safe data structures in C++ is essential.

### Resource Usage
-   **Memory:** Each new CEF window adds significant memory overhead (typically 50-100MB per process for the renderer and GPU process). However, subsequent windows from the same origin may share some resources.
-   **CPU:** The main overhead comes from `CefDoMessageLoopWork()` and the `OnPaint` callbacks. Because we use **Windowless Rendering (OSR)**, C++ must copy the pixel buffer for every frame the browser renders. 
-   **Optimization:** To save resources:
    1.  Reduce `windowless_frame_rate` for background windows.
    2.  Only call `UpdateCefTexture()` if the browser's `IsDirty()` flag is set.
    3.  Close browsers when they are not visible to kill their associated renderer processes.

---

## 5. CRUD Logic Flow

1.  **Create:** JS sends `action: "create"`. C++ appends to `std::vector`, returns success.
2.  **Read:** JS sends `action: "read"`. C++ iterates `std::vector`, builds JSON string, returns it.
3.  **Update:** JS sends `action: "update"` with `id` and new data. C++ finds item by `id`, updates it.
4.  **Delete:** JS sends `action: "delete"` with `id`. C++ removes item from `std::vector`.

## Advantages of this Approach

-   **No Network Overhead:** Communication happens via IPC (Inter-Process Communication), which is faster than local HTTP.
-   **Security:** No open ports or server-side vulnerabilities.
-   **Simplicity:** Clean separation of concerns between UI (HTML/JS) and Logic (C++).
-   **Performance:** C++ handles the heavy lifting (data storage, complex logic) while JS handles the rendering.
