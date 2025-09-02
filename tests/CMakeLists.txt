cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

# Find required packages for testing
find_package(Threads REQUIRED)

# CEF configuration (inherited from parent CMakeLists.txt)
# CEF_ROOT is already set by the parent CMakeLists.txt
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_LIBCEF_DLL_DIR "${CEF_ROOT}/libcef_dll")

# Platform-specific CEF library handling
if(UNIX AND NOT APPLE)
    set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
    set(CEF_LIBRARIES "${CEF_BINARY_DIR}/libcef.so")
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
    else()
        set(CEF_LIBRARIES
            "${CEF_BINARY_DIR}/libcef.lib"
        )
    endif()
endif()

# Test sources
set(TEST_SOURCES
    test_cef_initialize.cpp
)

# Create test executable
add_executable(test_cef_initialize ${TEST_SOURCES})

# Include directories
target_include_directories(test_cef_initialize PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CEF_INCLUDE_DIR}
)

# Link libraries (reuse the cef_dll_wrapper from parent)
target_link_libraries(test_cef_initialize PRIVATE
    cef_dll_wrapper
    ${CEF_LIBRARIES}
    Threads::Threads
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(test_cef_initialize PRIVATE dl)
endif()

# Compile definitions
target_compile_definitions(test_cef_initialize PRIVATE
    USING_CEF_SHARED
    WRAPPING_CEF_SHARED
)

# Set RPATH for Linux to find CEF shared library
if(UNIX AND NOT APPLE)
    set_target_properties(test_cef_initialize PROPERTIES
        INSTALL_RPATH "$ORIGIN/../"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Add test to CTest
add_test(NAME CEFInitializeTest COMMAND test_cef_initialize)

# Set environment variables for test
if(UNIX AND NOT APPLE)
    # Linux: Use LD_LIBRARY_PATH
    set_tests_properties(CEFInitializeTest PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
    )
elseif(WIN32)
    # Windows: Use PATH
    set_tests_properties(CEFInitializeTest PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_BINARY_DIR};$ENV{PATH}"
    )
endif()