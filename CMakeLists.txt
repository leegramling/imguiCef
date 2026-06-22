cmake_minimum_required(VERSION 3.20)
project(ImGuiCefVulkan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module for dependency management
include(FetchContent)
include(GNUInstallDirs)

# Find system packages that cannot be fetched
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# Fetch GLFW instead of requiring system installation
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

# Fetch Tracy Profiler
FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        v0.10
)

# Configure GLFW options before fetching
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Tracy options
option(TRACY_ENABLE "Enable Tracy profiler" ON)
set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Tracy does not provide an install toggle. Adding it with EXCLUDE_FROM_ALL
# keeps its development headers and static library out of this app's install.
FetchContent_GetProperties(tracy)
if(NOT tracy_POPULATED)
    FetchContent_Populate(tracy)
    add_subdirectory("${tracy_SOURCE_DIR}" "${tracy_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()

# CEF paths
set(IMGUICEF_DEFAULT_CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef_binary_133.4.8")
set(CEF_ROOT "${IMGUICEF_DEFAULT_CEF_ROOT}" CACHE PATH "Path to the CEF binary distribution root")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_LIBCEF_DLL_DIR "${CEF_ROOT}/libcef_dll")

if(NOT EXISTS "${CEF_INCLUDE_DIR}/cef_version.h")
    message(FATAL_ERROR
        "CEF headers not found under '${CEF_ROOT}'. "
        "Point CEF_ROOT at a valid CEF binary distribution root."
    )
endif()

file(STRINGS "${CEF_INCLUDE_DIR}/cef_version.h" CEF_VERSION_LINE
    REGEX "^#define CEF_VERSION "
    LIMIT_COUNT 1
)
string(REGEX REPLACE "^#define CEF_VERSION \"([^\"]+)\"$" "\\1"
    CEF_VERSION_STRING "${CEF_VERSION_LINE}"
)

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
set(COMMON_SOURCES
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
    set(CEF_DEVELOPMENT_RUNTIME_DIR "${CMAKE_BINARY_DIR}/cef")

    # Keep development builds small. CEF requires its core library, ICU data,
    # V8 snapshot and resources in one runtime directory, so expose the binary
    # distribution through symlinks instead of copying it beside each target.
    set(CEF_LEGACY_BUILD_FILES
        chrome-sandbox
        chrome_100_percent.pak
        chrome_200_percent.pak
        icudtl.dat
        libEGL.so
        libGLESv2.so
        libcef.so
        libvk_swiftshader.so
        libvulkan.so.1
        resources.pak
        snapshot_blob.bin
        v8_context_snapshot.bin
        vk_swiftshader_icd.json
    )
    foreach(CEF_LEGACY_BUILD_FILE IN LISTS CEF_LEGACY_BUILD_FILES)
        file(REMOVE "${CMAKE_BINARY_DIR}/${CEF_LEGACY_BUILD_FILE}")
    endforeach()
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/locales")

    file(MAKE_DIRECTORY "${CEF_DEVELOPMENT_RUNTIME_DIR}")
    file(GLOB CEF_DEVELOPMENT_BINARY_FILES LIST_DIRECTORIES FALSE
        "${CEF_BINARY_DIR}/*"
    )
    file(GLOB CEF_DEVELOPMENT_RESOURCE_FILES LIST_DIRECTORIES FALSE
        "${CEF_ROOT}/Resources/*"
    )
    foreach(CEF_RUNTIME_FILE IN LISTS
            CEF_DEVELOPMENT_BINARY_FILES CEF_DEVELOPMENT_RESOURCE_FILES)
        get_filename_component(CEF_RUNTIME_NAME "${CEF_RUNTIME_FILE}" NAME)
        set(CEF_RUNTIME_LINK "${CEF_DEVELOPMENT_RUNTIME_DIR}/${CEF_RUNTIME_NAME}")
        file(REMOVE "${CEF_RUNTIME_LINK}")
        file(CREATE_LINK "${CEF_RUNTIME_FILE}" "${CEF_RUNTIME_LINK}" SYMBOLIC)
    endforeach()
    file(REMOVE "${CEF_DEVELOPMENT_RUNTIME_DIR}/locales")
    file(CREATE_LINK
        "${CEF_ROOT}/Resources/locales"
        "${CEF_DEVELOPMENT_RUNTIME_DIR}/locales"
        SYMBOLIC
    )
    set(CEF_LIBRARIES "${CEF_DEVELOPMENT_RUNTIME_DIR}/libcef.so")
    
elseif(WIN32)
    # Windows - Set binary directory based on build configuration
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CEF_BINARY_DIR "${CEF_ROOT}/Debug")
        set(CEF_SANDBOX_LIB "${CEF_ROOT}/Debug/cef_sandbox.lib")
    else()
        set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
        set(CEF_SANDBOX_LIB "${CEF_ROOT}/Release/cef_sandbox.lib")
    endif()
    
    # Option to disable CEF sandbox (recommended for development)
    option(USE_CEF_SANDBOX "Enable CEF sandbox" OFF)
    
    if(USE_CEF_SANDBOX AND EXISTS "${CEF_SANDBOX_LIB}")
        set(CEF_LIBRARIES
            "${CEF_BINARY_DIR}/libcef.lib"
            "${CEF_SANDBOX_LIB}"
        )
        message(STATUS "CEF sandbox enabled - linking ${CEF_SANDBOX_LIB}")
    else()
        set(CEF_LIBRARIES
            "${CEF_BINARY_DIR}/libcef.lib"
        )
        if(USE_CEF_SANDBOX)
            message(WARNING "CEF sandbox requested but library not found: ${CEF_SANDBOX_LIB}")
        endif()
        message(STATUS "CEF sandbox disabled for easier development")
    endif()
    
    # Use POST_BUILD commands to copy files to the correct output directory
    # This ensures files go to Debug/ or Release/ subdirectory on Windows
    set(CEF_COPY_FILES
        "${CEF_BINARY_DIR}/libcef.dll"
    )
    
    # Add all DLLs from CEF binary directory
    file(GLOB CEF_DLLS "${CEF_BINARY_DIR}/*.dll")
    list(APPEND CEF_COPY_FILES ${CEF_DLLS})
    
    # Add additional Windows-specific CEF files
    set(CEF_ADDITIONAL_FILES
        "d3dcompiler_47.dll"
        "libEGL.dll" 
        "libGLESv2.dll"
        "vulkan-1.dll"
        "vk_swiftshader_icd.json"
        "snapshot_blob.bin"
        "v8_context_snapshot.bin"
    )
    
    foreach(FILE ${CEF_ADDITIONAL_FILES})
        if(EXISTS "${CEF_BINARY_DIR}/${FILE}")
            list(APPEND CEF_COPY_FILES "${CEF_BINARY_DIR}/${FILE}")
        endif()
    endforeach()
    
endif()

# Create executables
set(TARGETS ImGuiCefVulkan cefForms)
add_executable(ImGuiCefVulkan src/main.cpp ${COMMON_SOURCES} ${IMGUI_SOURCES})
add_executable(cefForms 
    src/cef_forms_main.cpp 
    src/cef_forms_app.cpp 
    src/cef_forms_client.cpp 
    ${COMMON_SOURCES} 
    ${IMGUI_SOURCES}
)

foreach(APP_TARGET ${TARGETS})
    # Include directories
    target_include_directories(${APP_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CEF_INCLUDE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${Vulkan_INCLUDE_DIRS}
    )

    # Link libraries
    target_link_libraries(${APP_TARGET} PRIVATE
        cef_dll_wrapper
        ${CEF_LIBRARIES}
        ${Vulkan_LIBRARIES}
        glfw
        Threads::Threads
    )

    if(TRACY_ENABLE)
        target_link_libraries(${APP_TARGET} PRIVATE Tracy::TracyClient)
        target_compile_definitions(${APP_TARGET} PRIVATE TRACY_ENABLE)
    endif()

    # Platform-specific libraries
    if(UNIX AND NOT APPLE)
        # Linux-specific libraries
        target_link_libraries(${APP_TARGET} PRIVATE dl X11)
    elseif(WIN32)
        # Windows-specific libraries for CEF and system integration
        target_link_libraries(${APP_TARGET} PRIVATE 
            user32 gdi32 shell32 ole32 oleaut32 uuid winmm
            advapi32 kernel32 ws2_32 psapi dbghelp comctl32
            winspool comdlg32 odbc32 odbccp32 shlwapi propsys
        )
    endif()

    # Compile definitions
    target_compile_definitions(${APP_TARGET} PRIVATE
        USING_CEF_SHARED
        WRAPPING_CEF_SHARED
    )

    # Windows-specific compile definitions
    if(WIN32)
        target_compile_definitions(${APP_TARGET} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
            _UNICODE
            UNICODE
        )
        
        # MSVC-specific compiler flags
        if(MSVC)
            target_compile_options(${APP_TARGET} PRIVATE
                /permissive-  # Conformance mode
                /W3           # Warning level
                /wd4996       # Disable deprecated function warnings
            )
        endif()

        # Keep resource packs separate from the executable during development.
        # An explicit dependency also stages them when an existing executable
        # does not need to be relinked after CMake changes.
        add_custom_target(${APP_TARGET}_cef_resources
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_TARGET}>/cef/locales"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_ROOT}/Resources/locales"
                "$<TARGET_FILE_DIR:${APP_TARGET}>/cef/locales"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Resources/chrome_100_percent.pak"
                "$<TARGET_FILE_DIR:${APP_TARGET}>/cef/chrome_100_percent.pak"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Resources/chrome_200_percent.pak"
                "$<TARGET_FILE_DIR:${APP_TARGET}>/cef/chrome_200_percent.pak"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Resources/resources.pak"
                "$<TARGET_FILE_DIR:${APP_TARGET}>/cef/resources.pak"
            COMMENT "Staging CEF resource bundles in $<TARGET_FILE_DIR:${APP_TARGET}>/cef"
            VERBATIM
        )
        add_dependencies(${APP_TARGET} ${APP_TARGET}_cef_resources)

    endif()

    # Set RPATH for Linux to find CEF shared library
    if(UNIX AND NOT APPLE)
        set_target_properties(${APP_TARGET} PROPERTIES
            BUILD_RPATH "$ORIGIN/cef"
            BUILD_RPATH_USE_ORIGIN TRUE
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH FALSE
        )
    endif()

    # Windows-specific POST_BUILD commands to copy CEF files to executable directory
    if(WIN32 AND DEFINED CEF_COPY_FILES)
        foreach(FILE ${CEF_COPY_FILES})
            if(EXISTS "${FILE}")
                get_filename_component(FILE_NAME ${FILE} NAME)
                add_custom_command(TARGET ${APP_TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FILE}"
                    "$<TARGET_FILE_DIR:${APP_TARGET}>/${FILE_NAME}"
                    COMMENT "Copying ${FILE_NAME} to executable directory"
                )
            endif()
        endforeach()
        
        # ICU must be available before application code can set CEF paths.
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Resources/icudtl.dat"
                "$<TARGET_FILE_DIR:${APP_TARGET}>/icudtl.dat"
            COMMENT "Copying CEF ICU data to executable directory"
        )
    endif()

endforeach()

# Copy assets folder to build directory ONLY for cefForms
add_custom_command(TARGET cefForms POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:cefForms>/assets"
    COMMENT "Copying assets directory to cefForms executable directory"
)

# Install a self-contained runtime. The executable and all CEF runtime files
# share one directory so no build-tree paths or compile-time install flag are
# required after relocation.
install(TARGETS ${TARGETS}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(UNIX AND NOT APPLE)
    set(CEF_INSTALL_BINARY_FILES ${CEF_DEVELOPMENT_BINARY_FILES})
    list(REMOVE_ITEM CEF_INSTALL_BINARY_FILES "${CEF_BINARY_DIR}/chrome-sandbox")
    install(FILES ${CEF_INSTALL_BINARY_FILES}
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    if(EXISTS "${CEF_BINARY_DIR}/chrome-sandbox")
        install(PROGRAMS "${CEF_BINARY_DIR}/chrome-sandbox"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
    install(FILES ${CEF_DEVELOPMENT_RESOURCE_FILES}
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${CEF_ROOT}/Resources/locales"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
elseif(WIN32)
    install(FILES ${CEF_COPY_FILES}
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(FILES
        "${CEF_ROOT}/Resources/icudtl.dat"
        "${CEF_ROOT}/Resources/chrome_100_percent.pak"
        "${CEF_ROOT}/Resources/chrome_200_percent.pak"
        "${CEF_ROOT}/Resources/resources.pak"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${CEF_ROOT}/Resources/locales"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)


message(STATUS "CEF Root: ${CEF_ROOT}")
message(STATUS "CEF Version: ${CEF_VERSION_STRING}")
message(STATUS "CEF Binary Dir: ${CEF_BINARY_DIR}")
message(STATUS "Building with full CEF integration")
if(DEFINED CEF_DEVELOPMENT_RUNTIME_DIR)
    message(STATUS "Development CEF runtime: ${CEF_DEVELOPMENT_RUNTIME_DIR}")
endif()

# Enable testing
enable_testing()

# Add tests subdirectory
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
