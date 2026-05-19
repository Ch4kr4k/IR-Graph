<#
.SYNOPSIS
    Easy Windows build + run entrypoint (MSVC).

.DESCRIPTION
    - Bootstraps vcpkg when missing.
    - Auto-detects Qt6 MSVC kit and LLVM/libclang.
    - Configures with Visual Studio generator + win_cmake.cmake.
    - Builds and optionally runs IR_Graph.exe.
#>
param(
    [ValidateSet("Debug","Release","RelWithDebInfo","MinSizeRel")]
    [string]$BuildType = "Release",
    [string]$QtPath = "",
    [string]$LLVMPath = "",
    [string]$VcpkgRoot = "",
    [string]$Arch = "x64",
    [switch]$Clean,
    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Info  { param([string]$m) Write-Host "[INFO]  $m" -ForegroundColor Cyan }
function Ok    { param([string]$m) Write-Host "[OK]    $m" -ForegroundColor Green }
function Warn  { param([string]$m) Write-Host "[WARN]  $m" -ForegroundColor Yellow }
function Fatal { param([string]$m) Write-Host "[ERROR] $m" -ForegroundColor Red; exit 1 }

function Require-Command {
    param([string]$Cmd, [string]$HelpMsg)
    if (-not (Get-Command $Cmd -ErrorAction SilentlyContinue)) {
        Fatal "'$Cmd' not found in PATH. $HelpMsg"
    }
}

Require-Command cmake "Install CMake from https://cmake.org/download/"
Require-Command git "Install Git from https://git-scm.com/download/win"

$repoRoot = $PSScriptRoot
$cacheInit = Join-Path $repoRoot "win_cmake.cmake"
if (-not (Test-Path $cacheInit)) { Fatal "Missing $cacheInit" }

# vcpkg (auto-bootstrap if needed)
$vcpkgCandidates = @(
    $VcpkgRoot,
    $env:VCPKG_ROOT,
    "$env:USERPROFILE\vcpkg",
    "C:\vcpkg",
    "C:\src\vcpkg",
    "C:\tools\vcpkg"
) | Where-Object { $_ -and (Test-Path "$_\vcpkg.exe" -ErrorAction SilentlyContinue) }

if ($vcpkgCandidates.Count -eq 0) {
    $VcpkgRoot = "$env:USERPROFILE\vcpkg"
    Warn "vcpkg not found, cloning to $VcpkgRoot"
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
    if ($LASTEXITCODE -ne 0) { Fatal "vcpkg bootstrap failed (exit $LASTEXITCODE)" }
} else {
    $VcpkgRoot = $vcpkgCandidates[0]
}
Ok "vcpkg: $VcpkgRoot"
$vcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# Qt6 MSVC kit discovery
if (-not $QtPath) {
    $qtRoots = @("C:\Qt", "$env:USERPROFILE\Qt", "D:\Qt") | Where-Object { Test-Path $_ }
    foreach ($qtRoot in $qtRoots) {
        $versions = Get-ChildItem $qtRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object Name -Descending
        foreach ($ver in $versions) {
            $kit = Get-ChildItem $ver.FullName -Directory -ErrorAction SilentlyContinue |
                Where-Object {
                    $_.Name -match '^msvc' -and
                    (Test-Path "$($_.FullName)\lib\cmake\Qt6\Qt6Config.cmake")
                } |
                Select-Object -First 1
            if ($kit) { $QtPath = $kit.FullName; break }
        }
        if ($QtPath) { break }
    }
}
if (-not $QtPath) {
    Fatal "Qt6 MSVC kit not found. Install Qt and pass -QtPath 'C:\Qt\6.x.x\msvc2022_64'"
}
Ok "Qt6: $QtPath"

# LLVM/libclang discovery
if (-not $LLVMPath) {
    $llvmCandidates = @(
        "C:\Program Files\LLVM",
        "C:\LLVM",
        "C:\tools\llvm"
    ) | Where-Object { Test-Path "$_\include\clang-c\Index.h" }

    $clangExe = Get-Command clang -ErrorAction SilentlyContinue
    if ($clangExe) {
        $candidate = Split-Path (Split-Path $clangExe.Source)
        if (Test-Path "$candidate\include\clang-c\Index.h") {
            $llvmCandidates = @($candidate) + $llvmCandidates
        }
    }
    if ($llvmCandidates.Count -gt 0) { $LLVMPath = $llvmCandidates[0] }
}
if (-not $LLVMPath) {
    Fatal "LLVM not found. Install LLVM and pass -LLVMPath 'C:\Program Files\LLVM'"
}
Ok "LLVM: $LLVMPath"

# Prefer VS 2022, fallback to VS 2019
$Generator = ""
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsYear = & $vswhere -latest -property catalog_productLineVersion 2>$null
    if ($vsYear -eq "2022") { $Generator = "Visual Studio 17 2022" }
    elseif ($vsYear -eq "2019") { $Generator = "Visual Studio 16 2019" }
}
if (-not $Generator) { $Generator = "Visual Studio 17 2022" }
Info "Generator: $Generator"

$buildDir = Join-Path $repoRoot "build-windows-$BuildType"
if ($Clean -and (Test-Path $buildDir)) {
    Info "Removing $buildDir"
    Remove-Item $buildDir -Recurse -Force
}

$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $buildDir,
    "-G", $Generator,
    "-A", $Arch,
    "-C", $cacheInit,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain",
    "-DCMAKE_PREFIX_PATH=$QtPath",
    "-DLLVM_DIR=$LLVMPath\lib\cmake\llvm",
    "-DClang_DIR=$LLVMPath\lib\cmake\clang",
    "-DLIBCLANG_INCDIR=$LLVMPath\include"
)

Info "Configuring..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Fatal "CMake configure failed (exit $LASTEXITCODE)" }
Ok "Configure succeeded"

Info "Building ($BuildType)..."
& cmake --build $buildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { Fatal "Build failed (exit $LASTEXITCODE)" }

$exePath = Join-Path $buildDir "$BuildType\IR_Graph.exe"
if (-not (Test-Path $exePath)) {
    $exePath = Join-Path $buildDir "IR_Graph.exe"
}
if (-not (Test-Path $exePath)) {
    Fatal "Build completed but IR_Graph.exe was not found."
}
Ok "Build complete: $exePath"

if ($Run) {
    Info "Running application..."
    & $exePath
} else {
    Write-Host ""
    Write-Host "Run with:" -ForegroundColor White
    Write-Host "  .\run.ps1 -BuildType $BuildType -Run" -ForegroundColor Gray
    Write-Host "or" -ForegroundColor White
    Write-Host "  & `"$exePath`"" -ForegroundColor Gray
}

