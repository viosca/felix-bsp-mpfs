# ==============================================================================
# prepare_sd.ps1 - Automated MicroSD Card Preparation for Felix OS on PolarFire SoC
# ==============================================================================
# This script configures a MicroSD card with:
#   - GPT Partition Table
#   - Partition 1 (64 MB): HSS Boot Partition (GUID: 21686148-6449-6e6f-744e-656564454649)
#     Contains raw payload.bin loaded by Hart Software Services into DDR at 0x80000000
#   - Partition 2 (Remainder): exFAT Basic Data Partition (Label: FELIX_SD)
#     Contains user files and Felix VFS storage
# ==============================================================================

[CmdletBinding()]
param(
    [int]$DiskNumber = 1
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Felix OS: MicroSD Preparation for PolarFire SoC HSS Boot " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Elevation Check
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script MUST be run as Administrator! Please right-click prepare_sd.bat and select 'Run as administrator', or launch an elevated PowerShell window."
    exit 1
}

# 2. Pre-flight Artifact Checks
$payloadPath = Join-Path $RepoRoot "payload.bin"
$backupDir = Join-Path $RepoRoot "scratch\sd_backup"

if (-not (Test-Path $payloadPath)) {
    Write-Error "Payload image not found: $payloadPath! Run scripts\generate_payload.ps1 first."
    exit 1
}

$payloadBytes = [System.IO.File]::ReadAllBytes($payloadPath)
Write-Host "[OK] Found $payloadPath ($($payloadBytes.Length) bytes)" -ForegroundColor Green

# 3. Disk Safety Verification
Write-Host "`nInspecting Disk $DiskNumber..." -ForegroundColor Yellow
$disk = Get-Disk -Number $DiskNumber -ErrorAction SilentlyContinue
if ($null -eq $disk) {
    Write-Error "Disk $DiskNumber not found! Please check that the MicroSD card reader is plugged in."
    exit 1
}

Write-Host "  Disk Friendly Name: $($disk.FriendlyName)"
Write-Host "  Bus Type:          $($disk.BusType)"
Write-Host "  Size:              $([math]::Round($disk.Size / 1GB, 2)) GB"
Write-Host "  IsSystem:          $($disk.IsSystem)"
Write-Host "  IsBoot:            $($disk.IsBoot)"

# Hard safety constraints
if ($disk.IsSystem -eq $true -or $disk.IsBoot -eq $true) {
    Write-Error "SAFETY ABORT: Disk $DiskNumber is a System or Boot disk! Never partition the system disk."
    exit 1
}

if ($disk.BusType -ne 'USB') {
    Write-Error "SAFETY ABORT: Disk $DiskNumber BusType is '$($disk.BusType)' (expected 'USB'). Aborting to protect internal drives."
    exit 1
}

if ($disk.Size -lt 4GB -or $disk.Size -gt 512GB) {
    Write-Error "SAFETY ABORT: Disk $DiskNumber size ($([math]::Round($disk.Size / 1GB, 2)) GB) is outside expected SD card range (4 GB - 512 GB)."
    exit 1
}

Write-Host "[OK] Disk $DiskNumber passed all safety checks!" -ForegroundColor Green

# 4. Dismount and Clean
Write-Host "`n[Step 1/5] Removing existing partitions on Disk $DiskNumber..." -ForegroundColor Yellow
$oldPartitions = Get-Partition -DiskNumber $DiskNumber -ErrorAction SilentlyContinue
if ($oldPartitions) {
    foreach ($p in $oldPartitions) {
        if ($p.DriveLetter) {
            Write-Host "  Removing drive letter $($p.DriveLetter):..."
            Remove-PartitionAccessPath -DiskNumber $DiskNumber -PartitionNumber $p.PartitionNumber -Accesspath "$($p.DriveLetter):" -ErrorAction SilentlyContinue
        }
    }
    $oldPartitions | Remove-Partition -Confirm:$false -ErrorAction SilentlyContinue
}

Write-Host "[Step 2/5] Initializing Disk $DiskNumber as GPT..." -ForegroundColor Yellow
$currentDisk = Get-Disk -Number $DiskNumber
if ($currentDisk.PartitionStyle -eq 'RAW') {
    Initialize-Disk -Number $DiskNumber -PartitionStyle GPT
} elseif ($currentDisk.PartitionStyle -ne 'GPT') {
    Set-Disk -Number $DiskNumber -PartitionStyle GPT
}

# 5. Create HSS Boot Partition (Partition 1: 64 MB, Type GUID: 21686148-6449-6e6f-744e-656564454649)
Write-Host "[Step 3/5] Creating 64 MB HSS Boot Partition (GUID: 21686148-6449-6e6f-744e-656564454649)..." -ForegroundColor Yellow
$hssGuid = "{21686148-6449-6e6f-744e-656564454649}"
$p1 = New-Partition -DiskNumber $DiskNumber -Size 64MB -GptType $hssGuid
Write-Host "  Partition 1 Start Offset: $($p1.Offset) bytes (LBA $($p1.Offset / 512))" -ForegroundColor Cyan

# 6. Write payload.bin directly into Partition 1
Write-Host "[Step 4/5] Writing payload.bin to Partition 1..." -ForegroundColor Yellow

# Align buffer to 512-byte boundary (required by Windows disk I/O)
$remainder = $payloadBytes.Length % 512
if ($remainder -ne 0) {
    $pad = 512 - $remainder
    $paddedBytes = New-Object byte[] ($payloadBytes.Length + $pad)
    [System.Buffer]::BlockCopy($payloadBytes, 0, $paddedBytes, 0, $payloadBytes.Length)
} else {
    $paddedBytes = $payloadBytes
}

Start-Sleep -Seconds 1

$rawDiskPath = "\\.\PhysicalDrive$DiskNumber"
Write-Host "  Opening $rawDiskPath at byte offset $($p1.Offset)..."
$stream = New-Object System.IO.FileStream($rawDiskPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
try {
    $stream.Seek($p1.Offset, [System.IO.SeekOrigin]::Begin) | Out-Null
    $stream.Write($paddedBytes, 0, $paddedBytes.Length)
    $stream.Flush($true)
    Write-Host "  [OK] Successfully wrote $($paddedBytes.Length) bytes ($($paddedBytes.Length / 512) sectors) to Partition 1!" -ForegroundColor Green
} finally {
    $stream.Close()
}

Start-Sleep -Seconds 1

# 7. Create and Format Data Partition (Partition 2: Remainder, exFAT, Label: FELIX_SD)
Write-Host "[Step 5/5] Creating user data partition..." -ForegroundColor Yellow
$p2 = New-Partition -DiskNumber $DiskNumber -UseMaximumSize -AssignDriveLetter
Start-Sleep -Seconds 1
$p2Updated = Get-Partition -DiskNumber $DiskNumber -PartitionNumber 2
$driveLetter = $p2Updated.DriveLetter

if ($driveLetter) {
    Write-Host "  Formatting drive $($driveLetter): as exFAT (Label: FELIX_SD)..."
    Format-Volume -DriveLetter $driveLetter -FileSystem exFAT -NewFileSystemLabel "FELIX_SD" -Confirm:$false | Out-Null
} else {
    Write-Host "  Formatting Partition 2 as exFAT (Label: FELIX_SD)..."
    Format-Volume -Partition $p2 -FileSystem exFAT -NewFileSystemLabel "FELIX_SD" -Confirm:$false | Out-Null
    Set-Partition -DiskNumber $DiskNumber -PartitionNumber 2 -NewDriveLetter 'D'
    $driveLetter = 'D'
}

Start-Sleep -Seconds 1
if (Test-Path $backupDir) {
    Write-Host "  Restoring backup files from $backupDir to $($driveLetter):\" -ForegroundColor Cyan
    Copy-Item -Path "$backupDir\*" -Destination "$($driveLetter):\" -Recurse -Force
}

# 8. Completion Summary
Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host "          SD CARD PREPARATION COMPLETED SUCCESSFULLY!      " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "`nFinal Partition Table on Disk $($DiskNumber):"
Get-Partition -DiskNumber $DiskNumber | Format-Table -AutoSize

Write-Host "`nFiles on $($driveLetter):\"
Get-ChildItem -Path "$($driveLetter):\" | Format-Table -AutoSize

Write-Host "`nNext Steps for Autonomous Boot:" -ForegroundColor Cyan
Write-Host "  1. Right-click drive $($driveLetter): in Windows Explorer and select 'Eject'."
Write-Host "  2. Remove the MicroSD card from your PC and insert it into the Discovery Kit slot."
Write-Host "  3. Open terminal on COM5 at 115200 baud for Felix OS (and optionally COM4 for HSS boot logs)."
Write-Host "  4. Press the RESET push-button on the Discovery Kit."
Write-Host "  5. HSS will discover the boot partition, load Felix OS to DDR (0x80000000), and boot!"
