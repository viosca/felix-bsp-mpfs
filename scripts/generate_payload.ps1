# ==============================================================================
# generate_payload.ps1 - Generate HSS payload.bin from felix_payload.yaml
# ==============================================================================
$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path

Push-Location $RepoRoot
try {
    $elfPath = Join-Path $RepoRoot "DDR-Debug-DiscoveryKit\felix-bsp-mpfs.elf"
    if (-not (Test-Path $elfPath)) {
        Write-Error "ELF binary not found at $elfPath. Please build DDR-Debug-DiscoveryKit first."
        exit 1
    }

    Write-Host "Generating payload.bin via hss-payload-generator..." -ForegroundColor Cyan
    $wslGenerator = "/mnt/c/git/hart-software-services/tools/hss-payload-generator/hss-payload-generator"
    & wsl $wslGenerator -c felix_payload.yaml payload.bin
    if ($LASTEXITCODE -ne 0) {
        Write-Error "hss-payload-generator failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    $size = (Get-Item (Join-Path $RepoRoot "payload.bin")).Length
    Write-Host "[OK] Generated payload.bin ($size bytes)" -ForegroundColor Green
} finally {
    Pop-Location
}
