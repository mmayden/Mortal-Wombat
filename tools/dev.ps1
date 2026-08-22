<#
.SYNOPSIS
    Runs a command inside the Visual Studio developer environment.

.DESCRIPTION
    VS 2022 Community ships cmake and ninja but does not put them (or the MSVC
    toolchain) on PATH. This wrapper imports the VS developer environment once
    and then execs whatever you passed it, so AGENTS.md can state exact
    commands that actually work on a clean Windows checkout.

    On Linux/macOS this script is unnecessary — call cmake/ctest directly.

.EXAMPLE
    powershell -File tools/dev.ps1 cmake --preset debug
    powershell -File tools/dev.ps1 ctest --preset debug -L unit
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
    [string[]] $Command
)

$ErrorActionPreference = 'Stop'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio 2022 with the 'Desktop development with C++' workload."
}

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) {
    throw "No Visual Studio install with the C++ toolchain was found. Install the 'Desktop development with C++' workload."
}

# Import the dev environment into this session. Launch-VsDevShell.ps1 is the
# supported entry point; -SkipAutomaticLocation keeps our working directory.
$devShell = Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1'
if (-not (Test-Path $devShell)) {
    throw "Launch-VsDevShell.ps1 not found under $vsPath"
}

$here = Get-Location
& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
Set-Location $here

# VS bundles cmake and ninja under CommonExtensions rather than on PATH.
$cmakeBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$ninjaBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
foreach ($bin in @($cmakeBin, $ninjaBin)) {
    if ((Test-Path $bin) -and ($env:PATH -notlike "*$bin*")) {
        $env:PATH = "$bin;$env:PATH"
    }
}

$exe = $Command[0]
$rest = if ($Command.Count -gt 1) { $Command[1..($Command.Count - 1)] } else { @() }

& $exe @rest
exit $LASTEXITCODE
