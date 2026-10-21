# Felix OS & PolarFire SoC Development TODO

This document tracks active development goals, architectural milestones, and planned tasks for Felix OS on the Microchip PolarFire SoC Discovery Kit.

---

## 1. DDR Execution & Boot Architecture

Resolve the memory bottleneck where PolarFire's 512KB LIM is insufficient for Felix OS + Toybox (which forced `HEAP_SIZE = 1k`).

- [x] **Analyze DDR Configuration (`mpfs-ddr-debug.ld` & `mss_sw_config.h`)**
  - [x] Linker script updated with `STACK_SIZE_E51_APPLICATION = 8k` and `HEAP_SIZE = 4M`.
  - [x] Configured `IMAGE_LOADED_BY_BOOTLOADER = 1` and `MPFS_HAL_FIRST_HART = 1`.
- [x] **Validate JTAG Debug Boot into DDR (Completed & Verified on Hardware)**
  - [x] Verified live execution via OpenOCD + GDB into external 1GB LPDDR4 at `0x80000000`.
  - [x] FreeRTOS shell operational on `COM4` (115200 baud).
  - [x] Standardize and configure SoftConsole Eclipse launch configurations in `launch_configs/debug/`:
    - `felix-disco-hw all-harts ddr debug.launch`: DDR load & debug on Hart 1 (`0x80000000`), preserving DDR training (`doFirstReset = false`).
    - `felix-disco-hw all-harts ddr attach.launch`: Non-intrusive live attach to Hart 1 in DDR.
    - `felix-disco-hw all-harts lim debug.launch`: Full silicon reset init and load into LIM (`0x08000000`).
    - `felix-disco-hw all-harts lim attach.launch`: Non-intrusive live attach to LIM.
    - Removed legacy ambiguous `attach.launch` and `debug.launch` files.
- [x] **Autonomous Standalone SD Boot via HSS (Milestone Achieved)**
  - [x] Flash official HSS reference bitstream `MPFS_DISCOVERY.job` (Libero bitstream + HSS v2026.04.1). Verified on hardware.
  - [x] Clone `hart-software-services` and compile `hss-payload-generator`.
  - [x] Back up MicroSD card (`hello.txt`, `zzz` to `scratch/sd_backup`).
  - [x] Create `felix_payload.yaml` and generate verified `payload.bin` (224 KB, `PRV_M`, `skip-opensbi`).
  - [x] Author automated preparation scripts `scripts/prepare_sd.ps1` and `scripts/prepare_sd.bat` (GPT dual-partition: 64MB HSS boot + ~238GB exFAT `FELIX_SD`).
  - [x] Successfully partition and flash MicroSD card on Windows.
  - [x] Verify autonomous boot into DDR at `0x80000000` via HSS on hardware.
  - [x] Reconfigure Felix console to `MMUART4` (`COM5`) to align with Discovery Kit reference design and eliminate contention with HSS on `COM4`.
  - [x] Author fast payload updater `scripts/update_payload.bat` / `scripts/update_payload.ps1` and `scripts/generate_payload.ps1`.
  - [x] Write updated `payload.bin` to SD card and verify interactive shell input on `COM5`.
  - [x] Diagnose missing `/dev/sd0p*` partition nodes: identified buffer overwrite bug in `part_gpt.c` and buffer alignment issues.
  - [x] Fix `part_gpt.c` header caching, add 64-bit alignment to `part_gpt.c` & `part_mbr.c`, rebuild ELF and regenerate `payload.bin`.
  - [x] Hardware verification: Partitions `/dev/sd0p1` and `/dev/sd0p2` discovered in `/dev`!
  - [x] Diagnose `Auto-mount failed for /dev/sd0p2 (Error -5)`:
    - `FF_VOLUMES` was set to `1` in `ffconf.h`, causing FatFs to reject logical drive `1:` (`/dev/sd0p2`) with `FR_INVALID_DRIVE`.
    - `disk_ioctl` in `fatfs_diskio.c` had hardcoded `pdrv != DEV_MMC (0)`, rejecting `pdrv = 1`.
  - [x] Fix `FF_VOLUMES = 10`, update `disk_ioctl` to support all dynamic volumes, fix `fat_to_errno` mapping and `%lu` format specifiers.
  - [x] Synthesize POSIX `.` and `..` entries in `fatfs_vfs_readdir()` with automatic directory rewind support.
  - [x] Standardize directory size to 4096 bytes (POSIX/Unix convention) in both `flx_vfs_stat()` and `fatfs_vfs_getattr()`.
  - [x] Hardware verification: `ls -al /mnt/sd0p2` displays `.` and `..` with size `4096`!
  - [x] Implement file manipulation builtins in `fs/rootfs/bin/flx_fs_bin_cmd_utils.c`:
    - `touch`: create empty files or update timestamps without truncating (`O_WRONLY | O_CREAT`).
    - `rm`: delete files or directories (`-f`, `-r`, `-R`) via `unlink()` and `rmdir()`.
    - `rmdir`: delete empty directories via `rmdir()`.
    - `cp`: copy files between paths and directories (`O_RDONLY` -> `O_WRONLY | O_CREAT | O_TRUNC`).
  - [x] Identify HSS boot payload partition GUID (`{21686148-6449-6e6f-744e-656564454649}`) in `part_gpt.c`:
    - Tag as `FLX_PART_TYPE_HSS_BOOT` (`0x48`).
    - Skip filesystem auto-mount in `blk_manager.c` to silence `Auto-mount failed for /dev/sd0p1 (Error -19)`.
  - [x] Rebuild `felix-proj-mpfs.elf` and regenerate `payload.bin`.
  - [x] Hardware verification on Discovery Kit:
    - [x] Clean boot: `/dev/sd0p1` tagged `[HSS Boot Payload]`, no Error -19 auto-mount warning.
    - [x] File operations on `/mnt/sd0p2`: `touch`, `cp`, `rm`, `ls -al` verified working on hardware.
  - [x] **Checkpoint 3: Git Commits & Merge to Main Completed**:
    - [x] `felix` submodule committed and pushed (`3f5cbc9` on `main`).
    - [x] Top repository fast-forward merged and pushed (`38cf262` on `main`).
    - [x] Deleted completed `feature/ddr-boot-fix` branch locally and remotely. Working trees 100% clean.
    - [x] Renamed project from `felix-proj-mpfs` to `felix-bsp-mpfs` in `.project`, `.cproject`, and `.launch` files.
    - [x] Restructured `src/application/` to `src/boot/` (`src/boot/hart0` through `hart4`) across code and build systems. Committed and pushed (`cd412e6` on `main`).
    - [x] Decoupled multi-repo architecture (`felix-bsp-mpfs`, `felix`, `mpfs-platform` as external sibling checkouts with transparent junctions).
    - [x] Added automated workspace setup scripts `setup_workspace.ps1` and `setup_workspace.sh`. Committed and pushed (`d97a319` on `main`).
    - [x] Resolved Toybox `generated/config.h` header dependency by tracking generated headers in `felix-port` branch of `viosca/toybox` (`b77b9491`).
    - [x] Updated `felix` submodule and `felix-bsp-mpfs` pointers; committed and pushed to remotes.
    - [x] Verified 100% clean builds for both `DDR-Debug-DiscoveryKit` and `LIM-Debug-DiscoveryKit` targets producing `felix-bsp-mpfs.elf`.
- [ ] **Define AMP Target Architecture (Asymmetric Multiprocessing)**
  - [ ] Partition Harts 1–3 for bare-metal / real-time control out of 512KB LIM.
  - [ ] Assign Hart 4 to DDR running Felix OS + Toybox supervisor.

---

## 2. FreeRTOS+POSIX Layer Integration

Modernize the POSIX API layer using the official FreeRTOS+POSIX library rather than hand-rolling individual POSIX wrappers. (See [`docs/freertos_posix_integration_plan.md`](file:///c:/git/felix-bsp-mpfs/docs/freertos_posix_integration_plan.md) for architectural details).

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

## 3. Configurability Subsystem (Kconfig)

Adopt the in-tree Kconfig system from `felix/external/toybox/kconfig` to generate compile-time profile headers (`autoconf.h`). (See [`docs/dynamic_loading_and_configurability_design.md`](file:///c:/git/felix-bsp-mpfs/docs/dynamic_loading_and_configurability_design.md)).

- [ ] **Kconfig Tooling Integration**
  - [ ] Wire build targets to compile host `conf` / `mconf` from `felix/external/toybox/kconfig/`.
  - [ ] Create root `Kconfig` for Felix defining memory models, loader options, and POSIX features.
- [ ] **Target Defconfigs**
  - [ ] Create `disco_lim_defconfig` (monolithic, <150 KB, deterministic real-time, no loader).
  - [ ] Create `disco_ddr_defconfig` (DDR memory, ELF loader, dynamic libraries, full POSIX).
  - [ ] Create `disco_amp_defconfig` (Harts 1-3 LIM control, Hart 4 DDR supervisor).
- [ ] **Header Generation**
  - [ ] Validate automated generation of `include/generated/autoconf.h`.

---

## 4. Position-Independent Dynamic ELF Loader & Shared Libraries (DLL / `.so`)

Enable loading external applications and runtime driver plugins from VFS into DDR RAM. (See [`docs/dynamic_loading_and_configurability_design.md`](file:///c:/git/felix-bsp-mpfs/docs/dynamic_loading_and_configurability_design.md)).

- [ ] **Kernel Symbol Export (`ksymtab`)**
  - [ ] Implement `FLX_EXPORT_SYMBOL()` registry for C runtime, POSIX, and FreeRTOS APIs.
  - [ ] Implement symbol lookup routine `flx_ksymtab_lookup(const char *name)`.
- [ ] **Static PIE ELF Loader Core (`flx_elf_loader.c`)**
  - [ ] Implement ELF64 header validation (`EM_RISCV`, `ET_DYN` / `ET_REL`).
  - [ ] Parse `PT_LOAD` segments and allocate memory span from DDR heap.
  - [ ] Implement relocation handler for RV64 (`R_RISCV_RELATIVE`, `R_RISCV_64`, `R_RISCV_JUMP_SLOT`).
  - [ ] Zero BSS sections (`.bss`, `.sbss`).
- [ ] **Process Execution Plumbing**
  - [ ] Implement `flx_proc_exec()` in `felix/kernel/process.c` called by `execvp()`.
  - [ ] Set up user process stack, `argc`, `argv`, and environment pointers.
  - [ ] Spawn execution thread via `sched_thread_create()`.
  - [ ] Reclaim allocated DDR memory and clean up PCB on `exit()`.
- [ ] **Dynamic Library / Plugin API (`dlopen` / `dlsym`)**
  - [ ] Implement `dlopen()` to load `.so` files into DDR and resolve against `ksymtab`.
  - [ ] Implement `dlsym()` to query function symbols from loaded module's `.dynsym`.
  - [ ] Implement `dlclose()` to invoke destructors and free memory.

---

## 5. Toybox Suite on SD Card (External Execution)

Decouple Toybox from the kernel binary and deploy it as standalone binaries in `/bin` on the FAT32 SD card partition.

- [ ] **Build Standalone Toybox PIE Binary**
  - [ ] Configure Toybox build with `-fPIE -pie` targeting RISC-V 64.
  - [ ] Link Toybox against Felix's exported symbols (or standalone static-PIE).
  - [ ] Place multicall `toybox` binary into `/bin/toybox` on the SD card image.
- [ ] **Enable Expanded Toybox Utility Suite**
  - [ ] File tools: `ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `touch`.
  - [ ] Text tools: `grep`, `sed`, `head`, `tail`, `wc`, `echo`, `sort`.
  - [ ] System info: `free`, `uname`, `uptime`, `ps`, `df`, `dd`.
- [ ] **Interactive Shell Integration**
  - [ ] Test command execution from `flx_sh` via `$PATH` resolution (`/bin/<cmd>`).
  - [ ] Test symlink or multicall dispatch (`/bin/ls` -> `/bin/toybox ls`).

---

## 6. VFS, Storage & Peripheral Maturation

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

## 7. Hardware Bench & Toolchain Testing

- [ ] **Flash & Run LIM Build on Board**
  - [ ] Load `LIM-Debug-DiscoveryKit` binary via SoftConsole JTAG.
  - [ ] Capture early UART boot logs and verify Felix shell prompt.
- [ ] **Test DDR Build on Board**
  - [ ] Load `DDR-Debug-DiscoveryKit` binary.
  - [ ] Check if E51 finishes DDR training and passes execution to Hart 1.
  - [ ] Check if trap occurs on DDR access (validating PMP configuration).
- [ ] **Headless Command-Line Build Scripts**
  - [ ] Create a PowerShell/Makefile helper to build targets without launching Eclipse GUI.
