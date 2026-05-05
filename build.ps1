<#
.SYNOPSIS
    Build IR_Graph on Windows.

.PARAMETER BuildType
    CMake build type: Debug or Release (default: Release)

.PARAMETER QtPath
    Path to the Qt6 MSVC kit, e.g. C:\Qt\6.7.0\msvc2022_64
    If omitted the script searches common install locations.

.PARAMETER LLVMPath
    LLVM install root, e.g. C:\Program Files\LLVM
    If omitted the script searches PATH and common locations.

.PARAMETER VcpkgRoot
    vcpkg root directory.  If omitted, $env:VCPKG_ROOT is used,
    then common locations are checked.  If still not found the
    script bootstraps a fresh clone under $env:USERPROFILE\vcpkg.

.PARAMETER Generator
    CMake generator: "Visual Studio 17 2022", "Visual Studio 16 2019",
    or "Ninja".  Auto-detected when omitted.

.PARAMETER Arch
    CMake platform for Visual Studio generators: x64 (default) or ARM64.

.PARAMETER Clean
    Delete the build directory before configuring.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -BuildType Debug -Clean
    .\build.ps1 -QtPath "C:\Qt\6.7.0\msvc2022_64" -LLVMPath "C:\Program Files\LLVM"
#>
param(
    [ValidateSet("Debug","Release","RelWithDebInfo","MinSizeRel")]
    [string]$BuildType = "Release",

    [string]$QtPath    = "",
    [string]$LLVMPath  = "",
    [string]$VcpkgRoot = "",
    [string]$Generator = "",
    [string]$Arch      = "x64",
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Helpers ──────────────────────────────────────────────────────────────────
function Info  { param([string]$m) Write-Host "[INFO]  $m" -ForegroundColor Cyan }
function Ok    { param([string]$m) Write-Host "[OK]    $m" -ForegroundColor Green }
function Warn  { param([string]$m) Write-Host "[WARN]  $m" -ForegroundColor Yellow }
function Fatal { param([string]$m) Write-Host "[ERROR] $m" -ForegroundColor Red; exit 1 }

function Require-Command {
    param([string]$Cmd, [string]$HelpMsg)
    if (-not (Get-Command $Cmd -ErrorAction SilentlyContinue)) {
        Fatal "'$Cmd' not found in PATH.  $HelpMsg"
    }
}

# ── Verify cmake ──────────────────────────────────────────────────────────────
Require-Command cmake  "Download from https://cmake.org/download/"
Require-Command git    "Download from https://git-scm.com/"

$cmakeVer = (cmake --version | Select-String '\d+\.\d+\.\d+').Matches[0].Value
Info "CMake $cmakeVer"

# ── Locate vcpkg ─────────────────────────────────────────────────────────────
$vcpkgCandidates = @(
    $VcpkgRoot,
    $env:VCPKG_ROOT,
    "$env:USERPROFILE\vcpkg",
    "C:\vcpkg",
    "C:\src\vcpkg",
    "C:\tools\vcpkg"
) | Where-Object { $_ -and (Test-Path "$_\vcpkg.exe" -ErrorAction SilentlyContinue) }

if ($vcpkgCandidates.Count -eq 0) {
    Warn "vcpkg not found — bootstrapping a fresh copy at $env:USERPROFILE\vcpkg"
    git clone https://github.com/microsoft/vcpkg.git "$env:USERPROFILE\vcpkg"
    & "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
    $VcpkgRoot = "$env:USERPROFILE\vcpkg"
} else {
    $VcpkgRoot = $vcpkgCandidates[0]
    Ok "vcpkg: $VcpkgRoot"
}

$vcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# ── Locate Qt6 ────────────────────────────────────────────────────────────────
if (-not $QtPath) {
    $qtRoots = @("C:\Qt", "$env:USERPROFILE\Qt", "D:\Qt") |
        Where-Object { Test-Path $_ }

    foreach ($qtRoot in $qtRoots) {
        # Find all version directories, pick the highest that has an MSVC kit
        $kits = Get-ChildItem $qtRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object Name -Descending

        foreach ($ver in $kits) {
            $msvc = Get-ChildItem $ver.FullName -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match 'msvc' -and (Test-Path "$($_.FullName)\lib\cmake\Qt6\Qt6Config.cmake") } |
                Select-Object -First 1
            if ($msvc) { $QtPath = $msvc.FullName; break }
        }
        if ($QtPath) { break }
    }
}

if ($QtPath) {
    Ok "Qt6: $QtPath"
} else {
    Fatal "Qt6 not found.`n  Install from https://www.qt.io/download-qt-installer`n  Then pass -QtPath 'C:\Qt\6.x.x\msvc2022_64'"
}

# ── Locate LLVM / libclang ────────────────────────────────────────────────────
if (-not $LLVMPath) {
    $llvmCandidates = @(
        "C:\Program Files\LLVM",
        "C:\LLVM",
        "C:\tools\llvm"
    ) | Where-Object { Test-Path "$_\include\clang-c\Index.h" }

    # Also check PATH
    $clangExe = Get-Command clang -ErrorAction SilentlyContinue
    if ($clangExe) {
        $candidate = Split-Path (Split-Path $clangExe.Source)
        if (Test-Path "$candidate\include\clang-c\Index.h") {
            $llvmCandidates = @($candidate) + $llvmCandidates
        }
    }

    if ($llvmCandidates.Count -gt 0) {
        $LLVMPath = $llvmCandidates[0]
    }
}

if ($LLVMPath) {
    Ok "LLVM: $LLVMPath"
} else {
    Fatal "LLVM not found.`n  Install: winget install LLVM.LLVM`n  Or download from https://releases.llvm.org/`n  Then pass -LLVMPath 'C:\Program Files\LLVM'"
}

# ── Choose CMake generator ────────────────────────────────────────────────────
if (-not $Generator) {
    # Prefer VS 2022, fall back to 2019, then Ninja
    $vsVersions = @(
        @{ Name = "Visual Studio 17 2022"; Reg = "HKLM:\SOFTWARE\Microsoft\VisualStudio\17.0" },
        @{ Name = "Visual Studio 16 2019"; Reg = "HKLM:\SOFTWARE\Microsoft\VisualStudio\16.0" }
    )
    foreach ($vs in $vsVersions) {
        if (Test-Path $vs.Reg -ErrorAction SilentlyContinue) {
            $Generator = $vs.Name
            break
        }
    }
    # Fallback: try vswhere
    if (-not $Generator) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsYear = & $vswhere -latest -property catalog_productLineVersion 2>$null
            if ($vsYear -eq "2022") { $Generator = "Visual Studio 17 2022" }
            elseif ($vsYear -eq "2019") { $Generator = "Visual Studio 16 2019" }
        }
    }
    # Final fallback: Ninja
    if (-not $Generator) {
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            $Generator = "Ninja"
        } else {
            Fatal "No suitable generator found.  Install Visual Studio 2019/2022 or Ninja."
        }
    }
}
Ok "Generator: $Generator"

# ── Paths ─────────────────────────────────────────────────────────────────────
$scriptDir = $PSScriptRoot
$buildDir  = Join-Path $scriptDir "build-windows-$BuildType"

if ($Clean -and (Test-Path $buildDir)) {
    Info "Removing $buildDir"
    Remove-Item $buildDir -Recurse -Force
}

# ── cmake configure ───────────────────────────────────────────────────────────
$cmakeArgs = @(
    "-S", $scriptDir,
    "-B", $buildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain",
    "-DCMAKE_PREFIX_PATH=$QtPath",
    "-DLLVM_DIR=$LLVMPath\lib\cmake\llvm",
    "-DClang_DIR=$LLVMPath\lib\cmake\clang",
    "-DLIBCLANG_INCDIR=$LLVMPath\include",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

if ($Generator -like "Visual Studio*") {
    $cmakeArgs += @("-G", $Generator, "-A", $Arch)
} else {
    $cmakeArgs += @("-G", $Generator)
    # Ninja needs an explicit compiler; try MSVC cl.exe via vcvars
    $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    }
    if (-not (Test-Path $vcvars)) {
        $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    }
    if (Test-Path $vcvars) {
        Info "Sourcing MSVC environment for Ninja build"
        # Import vcvars into current process via a temp batch file
        $tmpBat = [System.IO.Path]::GetTempFileName() + ".bat"
        @"
@call "$vcvars"
@set > "%TEMP%\vcvars_env.txt"
"@ | Set-Content $tmpBat
        cmd /c $tmpBat 2>$null
        Get-Content "$env:TEMP\vcvars_env.txt" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') {
                [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
            }
        }
        Remove-Item $tmpBat -ErrorAction SilentlyContinue
        Remove-Item "$env:TEMP\vcvars_env.txt" -ErrorAction SilentlyContinue
    }
}

Info "Configuring..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Fatal "CMake configure failed (exit $LASTEXITCODE)" }
Ok "Configure succeeded"

# ── cmake build ───────────────────────────────────────────────────────────────
$buildArgs = @(
    "--build", $buildDir,
    "--config", $BuildType,
    "--parallel"
)

Info "Building ($BuildType)..."
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { Fatal "Build failed (exit $LASTEXITCODE)" }

$exePath = Join-Path $buildDir "$BuildType\IR_Graph.exe"
if (-not (Test-Path $exePath)) {
    # Ninja / single-config generators put the binary directly in buildDir
    $exePath = Join-Path $buildDir "IR_Graph.exe"
}

if (Test-Path $exePath) {
    Ok "Build complete: $exePath"
} else {
    Ok "Build complete (binary location may vary)"
}

Write-Host ""
Write-Host "Run with:" -ForegroundColor White
Write-Host "  & `"$exePath`"" -ForegroundColor Gray
