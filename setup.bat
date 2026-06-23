@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Configure and build the Windows ImGuiCefVulkan target.
rem Usage:
rem   setup.bat [Debug^|Release] [build-dir] [cef-root]

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "BUILD_DIR=%~2"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build"

set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR:~0,-1%"
if not "%BUILD_DIR:~1,1%"==":" if not "%BUILD_DIR:~0,1%"=="\" (
    set "BUILD_DIR=%SOURCE_DIR%\%BUILD_DIR%"
)

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Invalid config "%CONFIG%". Use Debug or Release.
    exit /b 1
)

set "IMGUI_DIR=%SOURCE_DIR%\imgui"
set "IMGUI_READY="
if exist "%IMGUI_DIR%\imgui.cpp" if exist "%IMGUI_DIR%\imgui.h" if exist "%IMGUI_DIR%\backends\imgui_impl_glfw.cpp" if exist "%IMGUI_DIR%\backends\imgui_impl_vulkan.cpp" (
    set "IMGUI_READY=1"
)

if not defined IMGUI_READY (
    set "HAS_IMGUI_SUBMODULE="
    if exist "%SOURCE_DIR%\.gitmodules" (
        findstr /c:"path = imgui" "%SOURCE_DIR%\.gitmodules" >nul && set "HAS_IMGUI_SUBMODULE=1"
    )

    if defined HAS_IMGUI_SUBMODULE (
        where git >nul 2>nul
        if errorlevel 1 (
            echo ImGui files are missing from "%IMGUI_DIR%".
            echo The repository config expects an imgui submodule, but Git is not available in PATH.
            echo Run "git submodule update --init --recursive imgui" or copy the ImGui sources into "%IMGUI_DIR%".
            exit /b 1
        )

        echo ImGui files are missing. Attempting to initialize/update the imgui submodule...
        git -C "%SOURCE_DIR%" submodule update --init --recursive imgui
        if errorlevel 1 (
            echo Failed to initialize the imgui submodule.
            echo Run "git submodule update --init --recursive imgui" manually or copy the ImGui sources into "%IMGUI_DIR%".
            exit /b 1
        )

        if exist "%IMGUI_DIR%\imgui.cpp" if exist "%IMGUI_DIR%\imgui.h" if exist "%IMGUI_DIR%\backends\imgui_impl_glfw.cpp" if exist "%IMGUI_DIR%\backends\imgui_impl_vulkan.cpp" (
            set "IMGUI_READY=1"
        )
    )
)

if not defined IMGUI_READY (
    echo ImGui sources were not found under "%IMGUI_DIR%".
    echo setup.bat expects these files:
    echo   imgui.cpp
    echo   imgui.h
    echo   backends\imgui_impl_glfw.cpp
    echo   backends\imgui_impl_vulkan.cpp
    echo Initialize the imgui submodule or copy the ImGui repository contents into "%IMGUI_DIR%".
    exit /b 1
)

if "%VULKAN_ROOT%"=="" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do (
        if exist "C:\VulkanSDK\%%D\Include\vulkan\vulkan.h" if exist "C:\VulkanSDK\%%D\Lib\vulkan-1.lib" (
            set "VULKAN_ROOT=C:\VulkanSDK\%%D"
            goto :vulkan_root_ready
        )
    )
)

:vulkan_root_ready
if "%VULKAN_ROOT%"=="" (
    echo VULKAN_ROOT is not set. Set it to your Vulkan SDK directory.
    echo Example: set "VULKAN_ROOT=C:\VulkanSDK\1.3.290.0"
    exit /b 1
)

if not exist "%VULKAN_ROOT%\Include\vulkan\vulkan.h" (
    echo Vulkan headers were not found under "%VULKAN_ROOT%\Include".
    exit /b 1
)

if not exist "%VULKAN_ROOT%\Lib\vulkan-1.lib" (
    echo Vulkan library was not found at "%VULKAN_ROOT%\Lib\vulkan-1.lib".
    exit /b 1
)

set "VULKAN_SDK=%VULKAN_ROOT%"

if exist "%BUILD_DIR%\CMakeCache.txt" (
    set "CACHED_GENERATOR="
    set "CACHED_PLATFORM="
    for /f "tokens=1,* delims==" %%A in ('
        findstr /b /c:"CMAKE_GENERATOR:INTERNAL=" /c:"CMAKE_GENERATOR_PLATFORM:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"
    ') do (
        if "%%A"=="CMAKE_GENERATOR:INTERNAL" set "CACHED_GENERATOR=%%B"
        if "%%A"=="CMAKE_GENERATOR_PLATFORM:INTERNAL" set "CACHED_PLATFORM=%%B"
    )

    if defined CACHED_GENERATOR if /I not "!CACHED_GENERATOR!"=="Visual Studio 17 2022" (
        echo Build directory "%BUILD_DIR%" was configured with generator "!CACHED_GENERATOR!".
        echo setup.bat requires "Visual Studio 17 2022" with platform "x64".
        echo Use a different build directory or delete "%BUILD_DIR%\CMakeCache.txt" and "%BUILD_DIR%\CMakeFiles".
        exit /b 1
    )

    if /I "!CACHED_GENERATOR!"=="Visual Studio 17 2022" if /I not "!CACHED_PLATFORM!"=="x64" (
        echo Build directory "%BUILD_DIR%" was configured with platform "!CACHED_PLATFORM!".
        echo setup.bat requires platform "x64".
        echo Use a different build directory or delete "%BUILD_DIR%\CMakeCache.txt" and "%BUILD_DIR%\CMakeFiles".
        exit /b 1
    )
)

set "SELECTED_CEF_ROOT=%~3"
if "%SELECTED_CEF_ROOT%"=="" if not "%CEF_ROOT%"=="" set "SELECTED_CEF_ROOT=%CEF_ROOT%"
if "%SELECTED_CEF_ROOT%"=="" (
    for /d %%D in ("%SOURCE_DIR%\cef_binary_*_windows64") do (
        set "SELECTED_CEF_ROOT=%%~fD"
    )
)
if "%SELECTED_CEF_ROOT%"=="" if exist "%SOURCE_DIR%\cef_binary_133.4.8" (
    set "SELECTED_CEF_ROOT=%SOURCE_DIR%\cef_binary_133.4.8"
)

if "%SELECTED_CEF_ROOT%"=="" (
    echo CEF_ROOT was not provided and no cef_binary_*_windows64 directory was found.
    echo Pass it as the third argument or set the CEF_ROOT environment variable.
    exit /b 1
)

if not exist "%SELECTED_CEF_ROOT%\include\cef_version.h" (
    echo CEF headers were not found under "%SELECTED_CEF_ROOT%\include".
    exit /b 1
)

echo Source:    "%SOURCE_DIR%"
echo Build:     "%BUILD_DIR%"
echo Config:    "%CONFIG%"
echo CEF_ROOT:  "%SELECTED_CEF_ROOT%"
echo Vulkan:    "%VULKAN_ROOT%"

cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
    -DCEF_ROOT="%SELECTED_CEF_ROOT%" ^
    -DVulkan_INCLUDE_DIR="%VULKAN_ROOT%\Include" ^
    -DVulkan_LIBRARY="%VULKAN_ROOT%\Lib\vulkan-1.lib"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target ImGuiCefVulkan
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built "%BUILD_DIR%\%CONFIG%\ImGuiCefVulkan.exe"
endlocal
