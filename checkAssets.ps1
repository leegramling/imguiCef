param(
    [string]$Config = "Debug",
    [string]$BuildDir = (Join-Path $PSScriptRoot "build-cef143")
)

$ErrorActionPreference = "Stop"
$missing = $false

$exeDir = Join-Path $BuildDir $Config
$cefDir = Join-Path $BuildDir "$Config\cef"
$localesDir = Join-Path $cefDir "locales"
$exeLocalesDir = Join-Path $exeDir "locales"

function Require-File {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Write-Host "Missing file: $Path"
        $script:missing = $true
    }
}

function Require-Directory {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        Write-Host "Missing directory: $Path"
        $script:missing = $true
    }
}

Write-Host "Checking CEF 143 assets in:"
Write-Host "  build dir: $BuildDir"
Write-Host "  config:    $Config"

$rootDlls = @(
    "libcef.dll",
    "chrome_elf.dll",
    "d3dcompiler_47.dll",
    "dxcompiler.dll",
    "dxil.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "vk_swiftshader.dll",
    "vulkan-1.dll"
)

$cefResources = @(
    "chrome_100_percent.pak",
    "chrome_200_percent.pak",
    "resources.pak",
    "icudtl.dat",
    "v8_context_snapshot.bin",
    "vk_swiftshader_icd.json"
)

foreach ($dll in $rootDlls) {
    Require-File (Join-Path $BuildDir $dll)
}

Require-File (Join-Path $exeDir "ImGuiCefVulkan.exe")

# Current Windows runtime behavior still expects these packs in the executable
# directory even if a separate cef/ resource directory is also staged.
foreach ($asset in $cefResources) {
    Require-File (Join-Path $exeDir $asset)
}

Require-Directory $exeLocalesDir
Require-File (Join-Path $exeLocalesDir "en-US.pak")

Require-Directory $cefDir
foreach ($asset in $cefResources) {
    Require-File (Join-Path $cefDir $asset)
}

Require-Directory $localesDir
Require-File (Join-Path $localesDir "en-US.pak")

if (Test-Path -LiteralPath $exeLocalesDir -PathType Container) {
    $exeLocalePaks = Get-ChildItem -LiteralPath $exeLocalesDir -Filter *.pak -File
    if ($exeLocalePaks.Count -eq 0) {
        Write-Host "Missing locale packs: $exeLocalesDir contains no .pak files"
        $missing = $true
    }
}

if (Test-Path -LiteralPath $localesDir -PathType Container) {
    $localePaks = Get-ChildItem -LiteralPath $localesDir -Filter *.pak -File
    if ($localePaks.Count -eq 0) {
        Write-Host "Missing locale packs: $localesDir contains no .pak files"
        $missing = $true
    }
}

$rootSnapshot = Join-Path $BuildDir "snapshot_blob.bin"
$exeSnapshot = Join-Path $exeDir "snapshot_blob.bin"
$cefSnapshot = Join-Path $cefDir "snapshot_blob.bin"
if ((Test-Path -LiteralPath $rootSnapshot -PathType Leaf) -and
    -not (Test-Path -LiteralPath $exeSnapshot -PathType Leaf)) {
    Write-Host "Missing optional file present in build root: $exeSnapshot"
    $missing = $true
}

if ((Test-Path -LiteralPath $rootSnapshot -PathType Leaf) -and
    -not (Test-Path -LiteralPath $cefSnapshot -PathType Leaf)) {
    Write-Host "Missing optional file present in build root: $cefSnapshot"
    $missing = $true
}

if ($missing) {
    Write-Host "CEF 143 asset check failed."
    exit 1
}

Write-Host "CEF 143 asset check passed."
