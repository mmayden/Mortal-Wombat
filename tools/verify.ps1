<#
.SYNOPSIS
    Runs everything CI runs, locally, in the order CI runs it.

.DESCRIPTION
    One command so that "did I forget something?" stops being a question.

    This exists because of a measured, repeated failure: changes edited by
    script rather than by hand kept skipping clang-format, and CI kept catching
    it after a push and a two-minute wait. A gate you can forget is a gate that
    reports late.

    Checks, in the order a failure is cheapest to fix:
      1. clang-format   (seconds, and the most-forgotten step)
      2. sim boundary   (seconds, catches determinism violations)
      3. build          (Debug, warnings are errors)
      4. tests          (all four tiers)
      5. replay drift   (recordings must be re-recorded deliberately, not by accident)

    It does NOT run the release build or the cross-platform desync comparison.
    Those need other machines or a second configuration; CI owns them.

.EXAMPLE
    powershell -File tools/verify.ps1
    powershell -File tools/verify.ps1 -Fix      # reformat in place instead of only reporting
#>
[CmdletBinding()]
param([switch] $Fix)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$failures = @()
function Step($name) { Write-Host "`n=== $name ===" -ForegroundColor Cyan }

# --- 1. formatting -----------------------------------------------------------
Step 'clang-format'
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) {
    $candidate = Join-Path $env:APPDATA 'Python\Python310\Scripts\clang-format.exe'
    if (Test-Path $candidate) { $clangFormat = $candidate } 
}
if (-not $clangFormat) {
    Write-Host 'clang-format not found. Install the pinned version:' -ForegroundColor Yellow
    Write-Host '  pip install clang-format==22.1.8' -ForegroundColor Yellow
    $failures += 'clang-format missing'
} else {
    $exe = if ($clangFormat -is [string]) { $clangFormat } else { $clangFormat.Source }
    $sources = Get-ChildItem -Path src, tests -Recurse -Include *.h, *.cpp
    $unformatted = @()
    foreach ($file in $sources) {
        if ($Fix) {
            & $exe -i $file.FullName
        } else {
            & $exe --dry-run --Werror $file.FullName 2>$null
            if ($LASTEXITCODE -ne 0) { $unformatted += $file.FullName.Substring($repo.Length + 1) }
        }
    }
    if ($Fix) {
        Write-Host "reformatted $($sources.Count) file(s)" -ForegroundColor Green
    } elseif ($unformatted.Count -gt 0) {
        Write-Host "$($unformatted.Count) file(s) need formatting:" -ForegroundColor Red
        $unformatted | ForEach-Object { Write-Host "  $_" }
        Write-Host 'Run: powershell -File tools/verify.ps1 -Fix' -ForegroundColor Yellow
        $failures += 'formatting'
    } else {
        Write-Host "$($sources.Count) file(s) formatted correctly" -ForegroundColor Green
    }
}

# --- 2. sim boundary ---------------------------------------------------------
Step 'sim boundary'
& python (Join-Path $repo 'tests\check_sim_boundary.py') (Join-Path $repo 'src\sim')
if ($LASTEXITCODE -ne 0) { $failures += 'sim boundary' }

# --- 3 & 4. build and test ---------------------------------------------------
Step 'build (debug)'
& powershell -File (Join-Path $PSScriptRoot 'dev.ps1') cmake --build --preset debug
if ($LASTEXITCODE -ne 0) { $failures += 'build' }

Step 'tests'
& powershell -File (Join-Path $PSScriptRoot 'dev.ps1') ctest --preset debug --output-on-failure
if ($LASTEXITCODE -ne 0) { $failures += 'tests' }

# --- 5. replay drift ---------------------------------------------------------
Step 'replay recordings'
$drift = & git status --porcelain -- tests/replays/
if ($drift) {
    Write-Host 'Replay recordings differ from the committed ones:' -ForegroundColor Yellow
    $drift | ForEach-Object { Write-Host "  $_" }
    Write-Host ''
    Write-Host 'That is fine IF you changed behaviour on purpose -- commit the new' -ForegroundColor Yellow
    Write-Host 'recordings together with the change that caused them (AGENTS.md rule 8).' -ForegroundColor Yellow
    Write-Host 'If you did not expect this, something changed the simulation.' -ForegroundColor Yellow
} else {
    Write-Host 'recordings match the committed ones' -ForegroundColor Green
}

# --- verdict -----------------------------------------------------------------
Write-Host ''
if ($failures.Count -gt 0) {
    Write-Host "FAILED: $($failures -join ', ')" -ForegroundColor Red
    exit 1
}
Write-Host 'All local checks passed. CI still owns the release build,' -ForegroundColor Green
Write-Host 'the three-platform matrix, and the desync comparison.' -ForegroundColor Green
exit 0
