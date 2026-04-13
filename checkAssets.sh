#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$SCRIPT_DIR/build-cef143"}"
CONFIG="${1:-Debug}"
EXE_DIR="$BUILD_DIR/$CONFIG"
CEF_DIR="$BUILD_DIR/$CONFIG/cef"
LOCALES_DIR="$CEF_DIR/locales"
EXE_LOCALES_DIR="$EXE_DIR/locales"

missing=0

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "Missing file: $path"
    missing=1
  fi
}

require_dir() {
  local path="$1"
  if [[ ! -d "$path" ]]; then
    echo "Missing directory: $path"
    missing=1
  fi
}

echo "Checking CEF 143 assets in:"
echo "  build dir: $BUILD_DIR"
echo "  config:    $CONFIG"

root_dlls=(
  "libcef.dll"
  "chrome_elf.dll"
  "d3dcompiler_47.dll"
  "dxcompiler.dll"
  "dxil.dll"
  "libEGL.dll"
  "libGLESv2.dll"
  "vk_swiftshader.dll"
  "vulkan-1.dll"
)

cef_resources=(
  "chrome_100_percent.pak"
  "chrome_200_percent.pak"
  "resources.pak"
  "icudtl.dat"
  "v8_context_snapshot.bin"
  "vk_swiftshader_icd.json"
)

for dll in "${root_dlls[@]}"; do
  require_file "$BUILD_DIR/$dll"
done

require_file "$EXE_DIR/ImGuiCefVulkan.exe"

# Current Windows runtime behavior still expects these packs in the executable
# directory even if a separate cef/ resource directory is also staged.
for asset in "${cef_resources[@]}"; do
  require_file "$EXE_DIR/$asset"
done

require_dir "$EXE_LOCALES_DIR"
require_file "$EXE_LOCALES_DIR/en-US.pak"

require_dir "$CEF_DIR"
for asset in "${cef_resources[@]}"; do
  require_file "$CEF_DIR/$asset"
done

require_dir "$LOCALES_DIR"
require_file "$LOCALES_DIR/en-US.pak"

if [[ -d "$EXE_LOCALES_DIR" ]]; then
  shopt -s nullglob
  exe_locale_files=("$EXE_LOCALES_DIR"/*.pak)
  shopt -u nullglob
  if [[ ${#exe_locale_files[@]} -eq 0 ]]; then
    echo "Missing locale packs: $EXE_LOCALES_DIR contains no .pak files"
    missing=1
  fi
fi

if [[ -d "$LOCALES_DIR" ]]; then
  shopt -s nullglob
  locale_files=("$LOCALES_DIR"/*.pak)
  shopt -u nullglob
  if [[ ${#locale_files[@]} -eq 0 ]]; then
    echo "Missing locale packs: $LOCALES_DIR contains no .pak files"
    missing=1
  fi
fi

if [[ -f "$BUILD_DIR/snapshot_blob.bin" && ! -f "$EXE_DIR/snapshot_blob.bin" ]]; then
  echo "Missing optional file present in build root: $EXE_DIR/snapshot_blob.bin"
  missing=1
fi

if [[ -f "$BUILD_DIR/snapshot_blob.bin" && ! -f "$CEF_DIR/snapshot_blob.bin" ]]; then
  echo "Missing optional file present in build root: $CEF_DIR/snapshot_blob.bin"
  missing=1
fi

if [[ $missing -ne 0 ]]; then
  echo "CEF 143 asset check failed."
  exit 1
fi

echo "CEF 143 asset check passed."
