# ==============================================================================
# update_payload.ps1 - Fast Payload Updater for MicroSD Partition 1 (HSS Boot)
# ==============================================================================
param(
    [int]$DiskNumber = 1
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script MUST be run as Administrator! Please right-click update_payload.bat and select 'Run as administrator'."
    exit 1
}

$payloadPath = Join-Path $RepoRoot "payload.bin"
if (-not (Test-Path $payloadPath)) {
    Write-Error "Payload image not found: $payloadPath! Run scripts\generate_payload.ps1 first."
    exit 1
}

Write-Host "Updating payload.bin on Disk $DiskNumber Partition 1..." -ForegroundColor Yellow

$payloadBytes = [System.IO.File]::ReadAllBytes($payloadPath)

$p1 = Get-Partition -DiskNumber $DiskNumber -PartitionNumber 1
Write-Host "  Partition 1 Start Offset: $($p1.Offset) bytes"

$remainder = $payloadBytes.Length % 512
if ($remainder -ne 0) {
    $pad = 512 - $remainder
    $paddedBytes = New-Object byte[] ($payloadBytes.Length + $pad)
    [System.Buffer]::BlockCopy($payloadBytes, 0, $paddedBytes, 0, $payloadBytes.Length)
} else {
    $paddedBytes = $payloadBytes
}

$rawDiskPath = "\\.\PhysicalDrive$DiskNumber"
$stream = New-Object System.IO.FileStream($rawDiskPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
try {
    $stream.Seek($p1.Offset, [System.IO.SeekOrigin]::Begin) | Out-Null
    $stream.Write($paddedBytes, 0, $paddedBytes.Length)
    $stream.Flush($true)
    Write-Host "[OK] Successfully wrote $($paddedBytes.Length) bytes to Partition 1!" -ForegroundColor Green
} finally {
    $stream.Close()
}

Write-Host "`n[Done] Eject the MicroSD card, insert into Discovery Kit, and open COM5 at 115200 baud." -ForegroundColor Cyan
