# ==============================================================================
# setup_workspace.ps1 - Workspace Setup for Felix BSP for PolarFire SoC
# ==============================================================================
# This script configures the multi-repository workspace by ensuring that the
# core OS kernel (felix) and vendor HAL (mpfs-platform) are cloned as sibling
# repositories and linked into the BSP via NTFS directory junctions.
#
# No Administrator privileges or Developer Mode required!
# ==============================================================================

[CmdletBinding()]
param(
    [string]$FelixRepo = "https://github.com/viosca/felix.git",
    [string]$PlatformRepo = "https://github.com/polarfire-soc/platform.git",
    [string]$PlatformCommit = "9011c280851c0d4d00b8d17ccc404246354fe09c"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DevRoot = Split-Path -Parent $ScriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Felix BSP Workspace Setup (Windows / NTFS Junctions)    " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "BSP Directory : $ScriptDir"
Write-Host "Sibling Root  : $DevRoot"
Write-Host ""

# --- 1. Sibling Felix OS Kernel ---
$FelixDir = Join-Path $DevRoot "felix"
if (-not (Test-Path $FelixDir)) {
    Write-Host "[1/4] Cloning core Felix OS kernel into $FelixDir..." -ForegroundColor Yellow
    git clone --recursive $FelixRepo $FelixDir
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone Felix OS kernel." }
} else {
    Write-Host "[1/4] Found sibling Felix OS kernel at $FelixDir." -ForegroundColor Green
}

# --- 2. Sibling Microchip Platform HAL ---
$PlatformDir = Join-Path $DevRoot "mpfs-platform"
if (-not (Test-Path $PlatformDir)) {
    Write-Host "[2/4] Cloning Microchip mpfs-platform into $PlatformDir..." -ForegroundColor Yellow
    git clone $PlatformRepo $PlatformDir
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone mpfs-platform." }
    Write-Host "      Checking out verified commit $PlatformCommit..." -ForegroundColor Yellow
    git -C $PlatformDir checkout $PlatformCommit
} else {
    Write-Host "[2/4] Found sibling mpfs-platform at $PlatformDir." -ForegroundColor Green
}

# --- 3. Link Felix OS into BSP ---
$LocalFelix = Join-Path $ScriptDir "felix"
if (-not (Test-Path $LocalFelix)) {
    Write-Host "[3/4] Creating NTFS Directory Junction: felix -> $FelixDir..." -ForegroundColor Yellow
    New-Item -ItemType Junction -Path $LocalFelix -Target $FelixDir | Out-Null
    Write-Host "      Junction created successfully." -ForegroundColor Green
} else {
    Write-Host "[3/4] Link/folder for felix already exists." -ForegroundColor Green
}

# --- 4. Link mpfs-platform into BSP ---
$LocalPlatform = Join-Path $ScriptDir "mpfs-platform"
if (-not (Test-Path $LocalPlatform)) {
    Write-Host "[4/4] Creating NTFS Directory Junction: mpfs-platform -> $PlatformDir..." -ForegroundColor Yellow
    New-Item -ItemType Junction -Path $LocalPlatform -Target $PlatformDir | Out-Null
    Write-Host "      Junction created successfully." -ForegroundColor Green
} else {
    Write-Host "[4/4] Link/folder for mpfs-platform already exists." -ForegroundColor Green
}

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Green
Write-Host " Workspace setup complete!                                " -ForegroundColor Green
Write-Host " Both sibling repositories are linked and ready to build. " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
