# Felix OS & PolarFire SoC Development TODO

This document tracks active development goals, known issues, and planned milestones for Felix OS running on the Microchip PolarFire SoC Discovery Kit.

---

## 1. DDR Execution & Boot Architecture

Resolve the memory bottleneck where PolarFire's 512KB LIM is insufficient for Felix OS + Toybox (which forced `HEAP_SIZE = 1k`).

- [ ] **Analyze DDR Configuration (`mpfs-ddr-debug.ld` & `mss_sw_config.h`)**
  - [ ] Review `IMAGE_LOADED_BY_BOOTLOADER` definition in `src/boards/mpfs-discovery-kit/platform_config/ddr-debug/mpfs_hal_config/mss_sw_config.h`.
  - [ ] Verify whether `MPFS_HAL_HW_CONFIG` runs DDR training and clock initialization when booting via JTAG/OpenOCD.
- [ ] **Address PMP (Physical Memory Protection) Locks**
  - [ ] Identify PMP configuration applied by Hart 0 (`e51.c`) vs Hart-Specific PMP registers.
  - [ ] If booting via HSS (Hart Software Services), verify payload configuration (`config.yaml`) to ensure DDR regions are mapped with RWX permissions for Hart 1/Hart 4 and not locked (`L=1`) in a way that blocks OS execution.
- [ ] **Validate JTAG Debug Boot into DDR**
  - [ ] Test `felix-disco-hw all-harts debug.launch` with `DDR-Debug-DiscoveryKit` build artifact.
  - [ ] Verify Hart 0 trains DDR and Hart 1 wakes up successfully at DDR base (`0x80000000` or `0x1000000000`).
- [ ] **Define AMP Target Architecture (Asymmetric Multiprocessing)**
  - [ ] Partition Harts 1–3 for bare-metal / real-time control out of 512KB LIM.
  - [ ] Assign Hart 4 to DDR running Felix OS + Toybox supervisor.

---

## 2. FreeRTOS+POSIX Layer Integration

Modernize the POSIX API layer using the official FreeRTOS+POSIX library rather than hand-rolling individual POSIX wrappers. (See [`docs/freertos_posix_integration_plan.md`](file:///c:/git/bar/docs/freertos_posix_integration_plan.md) for architectural details).

- [ ] **Phase 1: Submodule & Directory Setup**
  - [ ] Add `FreeRTOS-Plus-POSIX` submodule under `felix/external/freertos-posix`.
  - [ ] Add `FreeRTOS-Plus-POSIX/include` and `FreeRTOS-Plus-POSIX/include/portable` to Eclipse CDT include paths.
- [ ] **Phase 2: Configuration & Portable Headers**
  - [ ] Create `FreeRTOS_POSIX_portable.h` defining PolarFire SoC / RISC-V types (`pthread_t`, `pthread_attr_t`, `pthread_mutex_t`, etc.).
  - [ ] Create `FreeRTOS_POSIX_portable_default.h` ensuring no collisions with GCC Newlib toolchain (`riscv64-unknown-elf-gcc`).
- [ ] **Phase 3: Selective Component Compilation**
  - [ ] Enable `FreeRTOS_POSIX_pthread.c` (threads, attributes, join/detach).
  - [ ] Enable `FreeRTOS_POSIX_pthread_mutex.c` and `FreeRTOS_POSIX_pthread_cond.c` (synchronization).
  - [ ] Enable `FreeRTOS_POSIX_sched.c` and `FreeRTOS_POSIX_semaphore.c`.
  - [ ] Keep file/storage I/O (`open`, `read`, `write`, `close`, `lseek`) routed to Felix VFS (`felix/syscalls.c`), keeping FreeRTOS POSIX I/O disabled.
- [ ] **Phase 4: Felix Kernel Refactor**
  - [ ] Refactor `felix/kernel/sched.c` and `felix/kernel/process.c` to use standard `pthread` calls.
  - [ ] Verify shell (`flx_sh`) runs commands as standard POSIX pthreads.

---

## 3. Toybox Integration & Expansion (in DDR)

With DDR memory available (expanding available RAM from <512KB to >32MB):

- [ ] **Heap Restoration**
  - [ ] Increase linker `HEAP_SIZE` from `1k` back to `32MB+` in `mpfs-ddr-debug.ld`.
- [ ] **Enable Standard Toybox Utilities**
  - [ ] Enable core file utilities: `ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `touch`.
  - [ ] Enable text processing: `grep`, `sed`, `head`, `tail`, `wc`, `echo`, `sort`.
  - [ ] Enable system info & diagnostic tools: `free`, `uname`, `uptime`, `ps`, `df`, `dd`.
- [ ] **Interactive Shell & Line Editing**
  - [ ] Verify `linenoise` integration inside `flx_sh`.
  - [ ] Test command history, autocomplete, and signal handling (`SIGINT` / Ctrl+C).

---

## 4. VFS, Storage & Peripheral Maturation

- [ ] **SD/eMMC Card Driver & Hotplugging**
  - [ ] Re-enable `flx_blk_manager_init()` and `blk_manager_daemon_task` in `init_task()`.
  - [ ] Verify MMC/SD interrupt handling and transfer rates on Discovery Kit.
- [ ] **Filesystem Partitioning & Mounting**
  - [ ] Verify MBR and 64-bit GPT partition table parsing on real SD card.
  - [ ] Auto-mount FAT32 partition to `/mnt/sd` or `/sdcard`.
- [ ] **Standard Dev Nodes**
  - [ ] Verify `/dev/console` bidirectional UART operations (input + output).
  - [ ] Implement/verify `/dev/null`, `/dev/zero`, and raw block device nodes (`/dev/mmcblk0`).

---

## 5. Hardware Bench & Toolchain Testing

- [ ] **Flash & Run LIM Build on Board**
  - [ ] Load `LIM-Debug-DiscoveryKit` binary via SoftConsole JTAG.
  - [ ] Capture early UART boot logs and verify Felix shell prompt.
- [ ] **Test DDR Build on Board**
  - [ ] Load `DDR-Debug-DiscoveryKit` binary.
  - [ ] Check if E51 finishes DDR training and passes execution to Hart 1.
  - [ ] Check if trap occurs on DDR access (validating PMP configuration).
- [ ] **Headless Command-Line Build Scripts**
  - [ ] Create a PowerShell/Makefile helper to build both targets without launching the Eclipse GUI.
