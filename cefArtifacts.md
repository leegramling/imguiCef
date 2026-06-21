# CEF Artifact Layout

This project uses different CEF artifact layouts for development builds and
installed applications. Development builds avoid copying the large CEF runtime
where possible. Installed applications are self-contained and do not depend on
the source tree or build tree.

The same executable supports both layouts. No compile definition is required
to distinguish a development executable from an installed executable.

## Source Distribution

`CEF_ROOT` must reference a CEF binary distribution with this basic layout:

```text
<CEF_ROOT>/
|-- Release/                 # Linux binaries, or Windows release binaries
|-- Debug/                   # Windows debug binaries, when available
|-- Resources/
|   |-- chrome_100_percent.pak
|   |-- chrome_200_percent.pak
|   |-- icudtl.dat
|   |-- resources.pak
|   `-- locales/
|-- include/
`-- libcef_dll/
```

The configured path can be changed with:

```bash
cmake -S . -B build -DCEF_ROOT=/absolute/path/to/cef
```

## Linux Development Layout

CEF artifacts are not copied beside the development executables. CMake creates
one `build/cef` directory containing symbolic links into the CEF distribution:

```text
build/
|-- ImGuiCefVulkan
|-- cefForms
|-- assets/                  # cefForms development assets
`-- cef/
    |-- libcef.so -> <CEF_ROOT>/Release/libcef.so
    |-- libEGL.so -> <CEF_ROOT>/Release/libEGL.so
    |-- libGLESv2.so -> <CEF_ROOT>/Release/libGLESv2.so
    |-- libvk_swiftshader.so -> <CEF_ROOT>/Release/libvk_swiftshader.so
    |-- libvulkan.so.1 -> <CEF_ROOT>/Release/libvulkan.so.1
    |-- v8_context_snapshot.bin -> <CEF_ROOT>/Release/v8_context_snapshot.bin
    |-- vk_swiftshader_icd.json -> <CEF_ROOT>/Release/vk_swiftshader_icd.json
    |-- chrome-sandbox -> <CEF_ROOT>/Release/chrome-sandbox
    |-- icudtl.dat -> <CEF_ROOT>/Resources/icudtl.dat
    |-- chrome_100_percent.pak -> <CEF_ROOT>/Resources/chrome_100_percent.pak
    |-- chrome_200_percent.pak -> <CEF_ROOT>/Resources/chrome_200_percent.pak
    |-- resources.pak -> <CEF_ROOT>/Resources/resources.pak
    `-- locales -> <CEF_ROOT>/Resources/locales
```

Optional artifacts such as `snapshot_blob.bin` are also linked when present in
the selected CEF distribution.

The development executables use this RUNPATH:

```text
$ORIGIN/cef
```

At runtime, the applications detect `cef/resources.pak` beside the executable
and set CEF's resource and locale paths to `cef/` and `cef/locales/`.

Because these entries are symbolic links, moving the build directory away from
the source CEF distribution can break them. Reconfigure with a valid `CEF_ROOT`
after moving either directory.

## Linux Install Layout

Installation copies the real CEF files into the same directory as the
executables. It does not install symbolic links into the CEF source tree.

```text
<prefix>/bin/
|-- ImGuiCefVulkan
|-- cefForms
|-- libcef.so
|-- libEGL.so
|-- libGLESv2.so
|-- libvk_swiftshader.so
|-- libvulkan.so.1
|-- v8_context_snapshot.bin
|-- vk_swiftshader_icd.json
|-- chrome-sandbox
|-- icudtl.dat
|-- chrome_100_percent.pak
|-- chrome_200_percent.pak
|-- resources.pak
|-- locales/
`-- assets/
```

The installed executables use `$ORIGIN` as their RUNPATH. When no `cef/`
resource directory exists beside an executable, the applications use the
executable directory itself for resources and `locales/` beneath it.

The applications currently set `CefSettings.no_sandbox = true`. Consequently,
the installed `chrome-sandbox` helper is not expected to provide setuid sandbox
operation.

## Windows Development Layout

Windows must load `libcef.dll` and its dependent DLLs before application code
can run. Those DLLs therefore remain beside each development executable. This
is a Windows loader requirement and cannot be solved with CEF resource flags.

For a multi-configuration generator, `<config>` is normally `Debug` or
`Release`:

```text
build/<config>/
|-- ImGuiCefVulkan.exe
|-- cefForms.exe
|-- libcef.dll
|-- libEGL.dll
|-- libGLESv2.dll
|-- other CEF runtime DLLs
|-- icudtl.dat
|-- v8_context_snapshot.bin        # when supplied by CEF
|-- vk_swiftshader_icd.json        # when supplied by CEF
|-- assets/                        # cefForms development assets
`-- cef/
    |-- chrome_100_percent.pak
    |-- chrome_200_percent.pak
    |-- resources.pak
    `-- locales/
```

Additional binary assets present in the selected CEF `Debug` or `Release`
directory are copied as needed. During development, the applications detect
`cef/resources.pak` and use `cef/` as the resource directory.

## Windows Install Layout

The Windows install is self-contained. DLLs, required data, resource packs,
locales, and assets are copied beside the installed executables:

```text
<prefix>/bin/
|-- ImGuiCefVulkan.exe
|-- cefForms.exe
|-- libcef.dll
|-- libEGL.dll
|-- libGLESv2.dll
|-- other CEF runtime DLLs
|-- icudtl.dat
|-- v8_context_snapshot.bin        # when supplied by CEF
|-- vk_swiftshader_icd.json        # when supplied by CEF
|-- chrome_100_percent.pak
|-- chrome_200_percent.pak
|-- resources.pak
|-- locales/
`-- assets/
```

With no `cef/resources.pak` directory in the installed layout, the applications
automatically select the executable directory and its `locales/` subdirectory.
No installed-only compile flag is necessary.

## Resource Overrides

`ImGuiCefVulkan` accepts explicit CEF resource locations on Linux:

```bash
./ImGuiCefVulkan \
  --resources-dir-path=/absolute/path/to/cef-runtime \
  --locales-dir-path=/absolute/path/to/cef-runtime/locales
```

These switches control resource packs and locales. They do not tell the
operating-system loader where to find `libcef.so` or `libcef.dll`.

On Linux, shared-library lookup is controlled by RUNPATH or
`LD_LIBRARY_PATH`. On Windows, DLL lookup is controlled by the executable
directory and the normal Windows DLL search rules.

`icudtl.dat` and the V8 snapshot are early runtime dependencies. They must be
available in the runtime layout expected by CEF; resource-path switches alone
are not a substitute for staging them correctly.

## Installing

Build and install with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /desired/prefix
```

After installation, the contents of `<prefix>/bin` can be moved together. The
executables must not be separated from their CEF libraries, data files,
resource packs, and locale directory.
