cmake_minimum_required(VERSION 3.20)
project(ImGuiCefVulkan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module for dependency management
include(FetchContent)

# Find system packages that cannot be fetched
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# Fetch GLFW instead of requiring system installation
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

# Configure GLFW options before fetching
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# CEF paths
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef_binary_133.4.8")
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

# Windows-specific compile definitions for CEF wrapper
if(WIN32)
    target_compile_definitions(cef_dll_wrapper PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _UNICODE
        UNICODE
    )
    
    if(MSVC)
        target_compile_options(cef_dll_wrapper PRIVATE
            /permissive-
            /W3
            /wd4996
        )
    endif()
endif()

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
    
    # Copy V8 snapshot files (if they exist - newer CEF versions may not have snapshot_blob.bin)
    if(EXISTS "${CEF_BINARY_DIR}/snapshot_blob.bin")
        configure_file("${CEF_BINARY_DIR}/snapshot_blob.bin" "${CMAKE_BINARY_DIR}/snapshot_blob.bin" COPYONLY)
    endif()
    if(EXISTS "${CEF_BINARY_DIR}/v8_context_snapshot.bin")
        configure_file("${CEF_BINARY_DIR}/v8_context_snapshot.bin" "${CMAKE_BINARY_DIR}/v8_context_snapshot.bin" COPYONLY)
    endif()
    
    # Copy required CEF resources
    message(STATUS "Copying CEF resources from: ${CEF_ROOT}/Resources/")
    file(COPY "${CEF_ROOT}/Resources/" DESTINATION "${CMAKE_BINARY_DIR}/")
    
    # Verify icudtl.dat was copied
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/icudtl.dat")
        message(WARNING "icudtl.dat not found in build directory after copy")
    else()
        message(STATUS "icudtl.dat successfully copied to build directory")
    endif()
    
    # Copy additional binaries if they exist
    if(EXISTS "${CEF_BINARY_DIR}/chrome-sandbox")
        configure_file("${CEF_BINARY_DIR}/chrome-sandbox" "${CMAKE_BINARY_DIR}/chrome-sandbox" COPYONLY)
    endif()
    
elseif(WIN32)
    # Windows - Set binary directory based on build configuration
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CEF_BINARY_DIR "${CEF_ROOT}/Debug")
        set(CEF_SANDBOX_LIB "${CEF_ROOT}/Debug/cef_sandbox.lib")
    else()
        set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
        set(CEF_SANDBOX_LIB "${CEF_ROOT}/Release/cef_sandbox.lib")
    endif()
    
    set(CEF_LIBRARIES
        "${CEF_BINARY_DIR}/libcef.lib"
        "${CEF_SANDBOX_LIB}"
    )
    
    # Copy CEF DLLs from the appropriate directory
    if(EXISTS "${CEF_BINARY_DIR}/libcef.dll")
        configure_file("${CEF_BINARY_DIR}/libcef.dll" "${CMAKE_BINARY_DIR}/libcef.dll" COPYONLY)
    endif()
    
    # Copy all DLLs from CEF binary directory
    file(GLOB CEF_DLLS "${CEF_BINARY_DIR}/*.dll")
    foreach(DLL ${CEF_DLLS})
        get_filename_component(DLL_NAME ${DLL} NAME)
        configure_file(${DLL} "${CMAKE_BINARY_DIR}/${DLL_NAME}" COPYONLY)
    endforeach()
    
    # Copy V8 snapshot files (if they exist)
    if(EXISTS "${CEF_BINARY_DIR}/snapshot_blob.bin")
        configure_file("${CEF_BINARY_DIR}/snapshot_blob.bin" "${CMAKE_BINARY_DIR}/snapshot_blob.bin" COPYONLY)
    endif()
    if(EXISTS "${CEF_BINARY_DIR}/v8_context_snapshot.bin")
        configure_file("${CEF_BINARY_DIR}/v8_context_snapshot.bin" "${CMAKE_BINARY_DIR}/v8_context_snapshot.bin" COPYONLY)
    endif()
    
    # Copy CEF resources
    message(STATUS "Copying CEF resources from: ${CEF_ROOT}/Resources/")
    file(COPY "${CEF_ROOT}/Resources/" DESTINATION "${CMAKE_BINARY_DIR}/")
    
    # Verify icudtl.dat was copied
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/icudtl.dat")
        message(WARNING "icudtl.dat not found in build directory after copy")
    else()
        message(STATUS "icudtl.dat successfully copied to build directory")
    endif()
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
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Linux-specific libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE dl X11)
elseif(WIN32)
    # Windows-specific libraries for CEF and system integration
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        user32 gdi32 shell32 ole32 oleaut32 uuid winmm
        advapi32 kernel32 ws2_32 psapi dbghelp comctl32
    )
endif()

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    USING_CEF_SHARED
    WRAPPING_CEF_SHARED
)

# Windows-specific compile definitions
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _UNICODE
        UNICODE
    )
    
    # MSVC-specific compiler flags
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            /permissive-  # Conformance mode
            /W3           # Warning level
            /wd4996       # Disable deprecated function warnings
        )
    endif()
endif()

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