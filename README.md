<div align="center">
  <img src="felix/docs/assets/felix_mascot.png" alt="Felix OS Mascot" width="160" />
  <h1>Felix OS — Microchip PolarFire SoC BSP</h1>
  <p><strong>Board Support Package and workspace wrapper for the Microchip PolarFire SoC Discovery Kit (64-bit RISC-V)</strong></p>
</div>

This repository is the **Board Support Package (BSP)** and SoftConsole/Eclipse workspace wrapper that runs [**Felix OS**](https://github.com/viosca/felix) on the **Microchip PolarFire SoC Discovery Kit** (64-bit RISC-V).

To keep Felix OS strictly hardware- and RTOS-agnostic, all PolarFire-specific startup code, multi-hart boot logic, memory layouts (512 KB LIM and 1 GB DDR), hardware drivers (MMUART and SD/MMC), and HSS SD-boot packaging live here—while the core OS lives cleanly in the [`felix`](felix/) repository.

---

## Quick Start

### 1. Prerequisites
* **Git** and **Python 3**
* **RISC-V GNU Cross-Compiler** (`riscv64-unknown-elf-gcc`) and `make` on your `PATH`
* *(Optional)* **Microchip SoftConsole / Eclipse CDT** for IDE builds and JTAG debugging
* *(Optional, for standalone SD boot)* **WSL + `hss-payload-generator`** to package `payload.bin`

### 2. Set Up the Workspace
Run the workspace setup script after cloning. It ensures the sibling [`felix`](https://github.com/viosca/felix) and [`mpfs-platform`](https://github.com/polarfire-soc/platform) repositories are present and linked into the project:

```powershell
# Windows (PowerShell - no Administrator rights required)
.\setup_workspace.ps1
```

```bash
# Linux / macOS
./setup_workspace.sh
```

### 3. Build Firmware

**From the Command Line:**
```powershell
# Build both DDR and LIM configurations
.\scripts\build.bat

# Build DDR and package HSS payload.bin for SD card boot
.\scripts\build.bat -Target ddr -Payload
```

**From SoftConsole / Eclipse CDT:**
1. Go to **File -> Import... -> General -> Existing Projects into Workspace**.
2. Select the `felix-bsp-mpfs` directory (leave **"Copy projects into workspace" unchecked**).
3. Select either `DDR-Debug-DiscoveryKit` or `LIM-Debug-DiscoveryKit` and click **Build**.
   *(Tip: If linked folders show a warning icon on first open, right-click the project and press **F5 / Refresh**.)*

### 4. Booting on the Discovery Kit
* **Standalone MicroSD Boot (HSS):** Prepare a card once with `scripts\prepare_sd.bat`, or update the boot image on an existing card with `scripts\update_payload.bat`. Connect a serial terminal (`115200 8N1`) to **MMUART4** for the interactive Felix shell (`MMUART0` shows HSS boot logs).
* **JTAG Debugging:** Pre-configured OpenOCD/GDB launch configurations for both DDR and LIM live in [`launch_configs/debug/`](launch_configs/debug/).

---

## Documentation

> **Note for Contributors & AI Assistants:** Before adding or modifying code across this workspace, review the **Felix Golden Rules** to ensure board-specific code stays in `felix-bsp-mpfs` and portable OS code stays in `felix`.

### BSP & Hardware Documentation (This Repo)
* **[PolarFire SoC Boot & Memory Architecture](docs/polarfire_soc_boot_architecture.md)** — E51/U54 multi-hart boot flow, LIM vs. DDR memory maps, HSS payload packaging, and JTAG debugging details.
* **[Development Roadmap & TODO](docs/TODO.md)** — Completed hardware milestones and active development tasks.

### Core Felix OS Documentation (`felix/` Submodule)
* **[Felix OS Overview (`felix/README.md`)](felix/README.md)** — High-level overview of Felix OS capabilities and configuration.
* **[Felix Golden Rules & Architectural Goals](felix/docs/GOLDEN_RULES.md)** — Core OS philosophy, hardware/RTOS quarantine rules, and AI coding guardrails.
* **[Dynamic Loading, Shared Libraries & Configurability](felix/docs/dynamic_loading_and_configurability_design.md)** — Kconfig system, PIE ELF loader, and shared library design.
* **[FreeRTOS+POSIX Integration Plan](felix/docs/freertos_posix_integration_plan.md)** — POSIX threads, synchronization, and IPC architecture.
