@echo off
setlocal EnableExtensions

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
