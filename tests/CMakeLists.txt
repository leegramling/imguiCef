cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

# Find required packages for testing
find_package(Threads REQUIRED)

# CEF configuration (reuse from parent)
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cef_binary_105.3.39")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_LIBCEF_DLL_DIR "${CEF_ROOT}/libcef_dll")

# Platform-specific CEF library handling
if(UNIX AND NOT APPLE)
    set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
    set(CEF_LIBRARIES "${CEF_BINARY_DIR}/libcef.so")
elseif(WIN32)
    set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
    set(CEF_LIBRARIES "${CEF_BINARY_DIR}/libcef.lib")
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
    dl
)

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
set_tests_properties(CEFInitializeTest PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
)