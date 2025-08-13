# GDB script for debugging CEF initialization test
# Usage: gdb -x debug_commands.gdb ./tests/test_cef_initialize

# Set environment
set environment LD_LIBRARY_PATH=/home/lgramling/dev/imguiCef/build

# Useful breakpoints for CEF debugging
break main
break CefInitialize
break CefExecuteProcess
break TestApp::OnBeforeCommandLineProcessing

# Handle CEF's multi-process nature
set follow-fork-mode child  # or 'parent' to stay with main process

# Show all loaded libraries
info sharedlibrary

# Run with arguments (if needed)
# set args --some-flag

# Start debugging
run