cmake_minimum_required(VERSION 3.20)
project(ImGuiCefVulkan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)

# CEF paths
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef_binary_105.3.39")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_LIBCEF_DLL_DIR "${CEF_ROOT}/libcef_dll")

# Add CEF CMake modules to path
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

# Find and configure CEF
find_package(CEF REQUIRED)

# ImGui configuration
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
if(NOT EXISTS "${IMGUI_DIR}")
    message(STATUS "ImGui not found, downloading...")
    execute_process(
        COMMAND git clone https://github.com/ocornut/imgui.git ${IMGUI_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Source files - USING FULL CEF INTEGRATION
set(SOURCES
    src/main.cpp
    src/vulkan_renderer.cpp
    src/cef_app.cpp
    src/cef_client.cpp
    src/imgui_layer.cpp
)

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# CEF wrapper sources
file(GLOB CEF_WRAPPER_SOURCES
    ${CEF_LIBCEF_DLL_DIR}/wrapper/*.cc
    ${CEF_LIBCEF_DLL_DIR}/base/*.cc
    ${CEF_LIBCEF_DLL_DIR}/cpptoc/*.cc
    ${CEF_LIBCEF_DLL_DIR}/ctocpp/*.cc
    ${CEF_LIBCEF_DLL_DIR}/*.cc
)

# Create CEF wrapper library
add_library(cef_dll_wrapper STATIC ${CEF_WRAPPER_SOURCES})
target_include_directories(cef_dll_wrapper PUBLIC 
    ${CEF_INCLUDE_DIR}
    ${CEF_ROOT}
)
target_compile_definitions(cef_dll_wrapper PUBLIC 
    USING_CEF_SHARED
    WRAPPING_CEF_SHARED
)

# Platform-specific CEF library handling
if(UNIX AND NOT APPLE)
    # Linux
    set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
    set(CEF_LIBRARIES
        "${CEF_BINARY_DIR}/libcef.so"
    )
    
    # Copy CEF binaries to output directory
    configure_file("${CEF_BINARY_DIR}/libcef.so" "${CMAKE_BINARY_DIR}/libcef.so" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/libGLESv2.so" "${CMAKE_BINARY_DIR}/libGLESv2.so" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/libEGL.so" "${CMAKE_BINARY_DIR}/libEGL.so" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/libvk_swiftshader.so" "${CMAKE_BINARY_DIR}/libvk_swiftshader.so" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/libvulkan.so.1" "${CMAKE_BINARY_DIR}/libvulkan.so.1" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/vk_swiftshader_icd.json" "${CMAKE_BINARY_DIR}/vk_swiftshader_icd.json" COPYONLY)
    
    # Copy V8 snapshot files
    configure_file("${CEF_BINARY_DIR}/snapshot_blob.bin" "${CMAKE_BINARY_DIR}/snapshot_blob.bin" COPYONLY)
    configure_file("${CEF_BINARY_DIR}/v8_context_snapshot.bin" "${CMAKE_BINARY_DIR}/v8_context_snapshot.bin" COPYONLY)
    
    # Copy required CEF resources
    file(COPY "${CEF_ROOT}/Resources/" DESTINATION "${CMAKE_BINARY_DIR}/")
    
    # Copy additional binaries if they exist
    if(EXISTS "${CEF_BINARY_DIR}/chrome-sandbox")
        configure_file("${CEF_BINARY_DIR}/chrome-sandbox" "${CMAKE_BINARY_DIR}/chrome-sandbox" COPYONLY)
    endif()
    
elseif(WIN32)
    # Windows
    set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
    set(CEF_LIBRARIES
        "${CEF_BINARY_DIR}/libcef.lib"
        "${CEF_ROOT}/Debug/cef_sandbox.lib"  # Use debug sandbox for now
    )
    
    # Copy CEF DLLs
    configure_file("${CEF_BINARY_DIR}/libcef.dll" "${CMAKE_BINARY_DIR}/libcef.dll" COPYONLY)
    file(GLOB CEF_RELEASE_DLLS "${CEF_BINARY_DIR}/*.dll")
    foreach(DLL ${CEF_RELEASE_DLLS})
        get_filename_component(DLL_NAME ${DLL} NAME)
        configure_file(${DLL} "${CMAKE_BINARY_DIR}/${DLL_NAME}" COPYONLY)
    endforeach()
    
    # Copy resources
    file(COPY "${CEF_ROOT}/Resources/" DESTINATION "${CMAKE_BINARY_DIR}/")
endif()

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CEF_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    cef_dll_wrapper
    ${CEF_LIBRARIES}
    ${Vulkan_LIBRARIES}
    glfw
    Threads::Threads
    dl
    X11
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    USING_CEF_SHARED
    WRAPPING_CEF_SHARED
)

# Set RPATH for Linux to find CEF shared library
if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# On Linux, set executable permissions for chrome-sandbox if it exists
if(UNIX AND NOT APPLE AND EXISTS "${CMAKE_BINARY_DIR}/chrome-sandbox")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND chmod 4755 "${CMAKE_BINARY_DIR}/chrome-sandbox"
        COMMENT "Setting chrome-sandbox permissions"
    )
endif()

message(STATUS "CEF Root: ${CEF_ROOT}")
message(STATUS "CEF Binary Dir: ${CEF_BINARY_DIR}")
message(STATUS "Building with full CEF integration")
message(STATUS "Make sure to run from build directory for proper resource loading")

# Enable testing
enable_testing()

# Add tests subdirectory
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()