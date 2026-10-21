#!/usr/bin/env bash
# ==============================================================================
# setup_workspace.sh - Workspace Setup for Felix BSP for PolarFire SoC (Linux/macOS)
# ==============================================================================
# This script ensures that the core OS kernel (felix) and vendor HAL
# (mpfs-platform) are cloned as sibling repositories and linked into the BSP
# via standard symbolic links.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV_ROOT="$(dirname "$SCRIPT_DIR")"

FELIX_REPO="${1:-https://github.com/viosca/felix.git}"
PLATFORM_REPO="${2:-https://github.com/polarfire-soc/platform.git}"
PLATFORM_COMMIT="${3:-9011c280851c0d4d00b8d17ccc404246354fe09c}"

echo "=========================================================="
echo " Felix BSP Workspace Setup (Linux / Symlinks)             "
echo "=========================================================="
echo "BSP Directory : $SCRIPT_DIR"
echo "Sibling Root  : $DEV_ROOT"
echo ""

# --- 1. Sibling Felix OS Kernel ---
FELIX_DIR="$DEV_ROOT/felix"
if [ ! -d "$FELIX_DIR" ]; then
    echo "[1/4] Cloning core Felix OS kernel into $FELIX_DIR..."
    git clone --recursive "$FELIX_REPO" "$FELIX_DIR"
else
    echo "[1/4] Found sibling Felix OS kernel at $FELIX_DIR."
fi

# --- 2. Sibling Microchip Platform HAL ---
PLATFORM_DIR="$DEV_ROOT/mpfs-platform"
if [ ! -d "$PLATFORM_DIR" ]; then
    echo "[2/4] Cloning Microchip mpfs-platform into $PLATFORM_DIR..."
    git clone "$PLATFORM_REPO" "$PLATFORM_DIR"
    echo "      Checking out verified commit $PLATFORM_COMMIT..."
    git -C "$PLATFORM_DIR" checkout "$PLATFORM_COMMIT"
else
    echo "[2/4] Found sibling mpfs-platform at $PLATFORM_DIR."
fi

# --- 3. Link Felix OS into BSP ---
LOCAL_FELIX="$SCRIPT_DIR/felix"
if [ ! -e "$LOCAL_FELIX" ]; then
    echo "[3/4] Creating symbolic link: felix -> $FELIX_DIR..."
    ln -s "$FELIX_DIR" "$LOCAL_FELIX"
    echo "      Symlink created successfully."
else
    echo "[3/4] Link/folder for felix already exists."
fi

# --- 4. Link mpfs-platform into BSP ---
LOCAL_PLATFORM="$SCRIPT_DIR/mpfs-platform"
if [ ! -e "$LOCAL_PLATFORM" ]; then
    echo "[4/4] Creating symbolic link: mpfs-platform -> $PLATFORM_DIR..."
    ln -s "$PLATFORM_DIR" "$LOCAL_PLATFORM"
    echo "      Symlink created successfully."
else
    echo "[4/4] Link/folder for mpfs-platform already exists."
fi

echo ""
echo "=========================================================="
echo " Workspace setup complete!                                "
echo " Both sibling repositories are linked and ready to build. "
echo "=========================================================="
