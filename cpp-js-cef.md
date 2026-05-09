# C++ and ReactJS Reactive Architecture in CEF (`cefForms`)

This document provides a comprehensive guide for developers adding new C++ logic and designers building modern UIs in the `cefForms` ecosystem. The architecture combines high-performance C++ simulations with a reactive ReactJS/TailwindCSS frontend.

---

## I. Architecture Overview: The "Push" vs "Pull" Models

Our application uses two distinct communication patterns:

1.  **The "Push" Model (State Sync):** C++ acts as the **Source of Truth**. When simulation data changes, C++ "pushes" the entire state to JavaScript. This is used for real-time updates (e.g., truck locations, system stats).
2.  **The "Pull" Model (Commands):** JavaScript sends a request to C++ (e.g., "Skip Delivery" or "Add Todo"). C++ processes the command, updates the model, and the next "Push" tick reflects the change in the UI.

---

## II. For the C++ Developer: Building the Backend

### 1. Defining the Data Model
Always use a thread-safe structure. High-frequency simulations should use `std::recursive_mutex` to avoid deadlocks during serialization.

```cpp
struct MyData {
    int id;
    std::string name;
};

std::vector<MyData> g_MyModels;
std::recursive_mutex g_ModelMutex; // Critical for nested locks
```

### 2. Implementing the IPC Handler
To receive commands from JavaScript, implement a handler. 
**CRUCIAL:** On Linux, you must use **Dual Inheritance** from `CefMessageRouterBrowserSide::Handler` and `CefBaseRefCounted`, and include the `IMPLEMENT_REFCOUNTING` macro.

```cpp
class MyHandler : public CefMessageRouterBrowserSide::Handler, public CefBaseRefCounted {
public:
    virtual bool OnQuery(...) override {
        std::lock_guard<std::recursive_mutex> lock(g_ModelMutex);
        // Process JSON request...
        return true;
    }
private:
    IMPLEMENT_REFCOUNTING(MyHandler); // Required for smart pointer safety
};
```

### 3. Broadcasting State (The Push)
Use `ExecuteJavaScript` to send JSON data to the frontend.

```cpp
void Application::PushStateToJS() {
    std::string json = SerializeModel(); // Convert vector to JSON string
    auto frame = browser->GetMainFrame();
    if (frame) {
        // Calls a global function defined in your HTML/React code
        std::string js = "if(window.updateState) { window.updateState(" + json + "); }";
        frame->ExecuteJavaScript(js, frame->GetURL(), 0);
    }
}
```

---

## III. For the UI Designer: Building the Frontend

### 1. React & Tailwind Integration
We use a "drop-in" React setup for simplicity, allowing you to edit HTML files directly without a complex build step.

```html
<script src="https://unpkg.com/react@18/umd/react.development.js"></script>
<script src="https://cdn.tailwindcss.com"></script>
```

### 2. The Reactive Event Pattern
React components should listen for the "Push" from C++. The easiest way is to bind a function to the `window` object.

```javascript
const { useState, useEffect } = React;

function MyDashboard() {
    const [data, setData] = useState([]);

    useEffect(() => {
        // C++ calls this function via ExecuteJavaScript
        window.updateState = (newData) => {
            setData(newData); // React magically updates the UI
        };
    }, []);

    return (
        <div className="bg-slate-900 p-4">
            {data.map(item => <div key={item.id}>{item.name}</div>)}
        </div>
    );
}
```

### 3. Sending Commands back to C++
Use `window.cefQuery` to trigger actions in the C++ backend.

```javascript
const handleAction = (id) => {
    window.cefQuery({
        request: JSON.stringify({ 
            action: 'my_command', 
            data: { id: id } 
        }),
        onSuccess: (res) => console.log("Success!"),
        onFailure: (err) => console.error(err)
    });
};
```

---

## IV. Critical Deployment Notes

### 1. Absolute Paths (Linux Stability)
CEF on Linux **requires absolute paths** for its resources and cache. Always use `std::filesystem::absolute()` when initializing `CefSettings`.

### 2. Multi-Window Isolation
Each browser window (Todo, Delivery, etc.) should have its own:
*   `BrowserInstance` (with its own `VkImage` and `DescriptorSet`).
*   Dedicated `CefMessageRouter` handler.
*   Independent React Root in its respective HTML file.

### 3. The "Single Source of Truth" Philosophy
**Never** try to maintain duplicate state in both C++ and JS.
*   Update the **C++ Model** first.
*   Let the **Push Model** update the JS UI.
*   This ensures that the UI always accurately reflects the simulation state.
