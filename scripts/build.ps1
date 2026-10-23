# ==============================================================================
# build.ps1 - Headless CLI Build Helper for Felix OS (DDR & LIM Targets)
# ==============================================================================
param(
    [ValidateSet("ddr", "lim", "all")]
    [string]$Target = "all",
    [switch]$Clean,
    [switch]$Payload
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path

Push-Location $RepoRoot
try {
    if (Test-Path (Join-Path $RepoRoot ".config")) {
        Write-Host "[Build] Synchronizing Kconfig header (autoconf.h)..." -ForegroundColor Cyan
        & python "felix/scripts/kconfig.py" genheader
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    $configs = @()
    if ($Target -eq "ddr" -or $Target -eq "all") { $configs += "DDR-Debug-DiscoveryKit" }
    if ($Target -eq "lim" -or $Target -eq "all") { $configs += "LIM-Debug-DiscoveryKit" }

    foreach ($cfg in $configs) {
        if ($Clean) {
            Write-Host "[Build] Cleaning $cfg..." -ForegroundColor Yellow
            & make -C $cfg clean
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        Write-Host "[Build] Compiling $cfg..." -ForegroundColor Cyan
        & make -C $cfg -j8 all
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed for $cfg (exit code $LASTEXITCODE)"
            exit $LASTEXITCODE
        }
        Write-Host "[OK] $cfg built successfully." -ForegroundColor Green
    }

    if ($Payload -and ($Target -eq "ddr" -or $Target -eq "all")) {
        & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\generate_payload.ps1"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
} finally {
    Pop-Location
}
