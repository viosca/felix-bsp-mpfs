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

- [x] **Phase 1: Submodule & Directory Setup**
  - [x] Add `FreeRTOS-Plus-POSIX` submodule under `felix/external/FreeRTOS-Plus-POSIX`.
  - [x] Add `FreeRTOS-Plus-POSIX` private/portable headers to Eclipse CDT include paths and expose non-colliding wrapper headers in `felix/include/`.
- [x] **Phase 2: Configuration & Portable Headers**
  - [x] Create `felix/osal/freertos/FreeRTOS_POSIX_portable.h` defining PolarFire SoC / 64-bit RISC-V types (`pthread_t`, `pthread_attr_t`, `pthread_mutex_t`, `pthread_cond_t`, `pthread_barrier_t`, `sem_t`, `timer_t`).
  - [x] Resolve 32-bit Newlib `<sys/_pthreadtypes.h>` truncation and `<errno.h>` / `<fcntl.h>` / `<signal.h>` / `<time.h>` collisions with `riscv64-unknown-elf-gcc`.
- [x] **Phase 3: Kconfig-Driven Selective Component Compilation**
  - [x] Create single translation-unit bridge `felix/osal/freertos/freertos_posix.c` gated by Kconfig (`CONFIG_FELIX_FREERTOS_PLUS_POSIX`, `CONFIG_FELIX_POSIX_PTHREAD`, `CONFIG_FELIX_POSIX_SEMAPHORE`, `CONFIG_FELIX_POSIX_MQUEUE`, `CONFIG_FELIX_POSIX_TIMER`).
  - [x] Enable `FreeRTOS_POSIX_pthread.c`, `FreeRTOS_POSIX_pthread_mutex.c`, `FreeRTOS_POSIX_pthread_cond.c`, and `FreeRTOS_POSIX_pthread_barrier.c`.
  - [x] Enable `FreeRTOS_POSIX_sched.c`, `FreeRTOS_POSIX_semaphore.c`, `FreeRTOS_POSIX_mqueue.c`, and `FreeRTOS_POSIX_timer.c`.
  - [x] Keep file/storage I/O (`open`, `read`, `write`, `close`, `lseek`) routed to Felix VFS (`felix/kernel/syscalls.c`) and inherit parent `FlxProcPCB_t *` across `pthread_create()` via `traceTASK_CREATE`.
  - [x] Implement and verify `/test/posix_test` in `felix/fs/rootfs/test/flx_fs_test.c`:
    - [x] Expanded to test `pthread_create`/`join` (64-bit return values), `pthread_mutex_lock`/`unlock`, `pthread_cond_wait`/`signal`, `pthread_barrier_wait`, `sem_init`/`post`/`wait`, `mqueue` (`mq_open`/`send`/`receive`/`close`/`unlink`), `timer_create`/`settime` (`SIGEV_THREAD`), and VFS coexistence across threads.
  - [x] Design and implement DRY header-only test framework [`felix/include/flx_test.h`](file:///c:/git/felix/include/flx_test.h):
    - [x] State tracking via `FlxTestCtx_t`, relational comparison operators (`"=="`, `"!="`, `"<"`, `"<="`, `">"`, `">="`), call-site location latching/clearing, and standardized CLI options parsing (`-v`, `-F`, `-q`, `-h`).
  - [x] Implement complete `/test` command suite modularized into separate source files in [`felix/fs/rootfs/test/`](file:///c:/git/felix/fs/rootfs/test/):
    - [x] `/test/mem_test`: `malloc` (8-byte alignment), `calloc` (zero-fill), `realloc` (retention), `strdup`, and multi-block interleaved stress.
    - [x] `/test/dev_test`: `/dev/console` stat/major/minor (`4, 4`), `/dev` enumeration, `uname()`, `setenv`/`getenv`/`unsetenv`, and monotonic clock progression.
    - [x] `/test/vfs_test`: file creation/write, `stat`/`fstat`, `lseek`/`read`, `mkdir`/`opendir`/`readdir`, `fstatat`/`unlinkat`/`rmdir` cleanup, plus custom `-d <dir>` target option.
    - [x] `/test/posix_test`: `pthread_create`/`join` (64-bit retval), mutex contention, condition variables, barrier synchronization, semaphores, message queues, and interval timers.
    - [x] `/test/wordexp_test`: word splitting, single/double quotes, escape characters, environment variable expansion, and `execvp` path resolution.
    - [x] `/test/io_test`: `O_APPEND` semantic guarantees, `ftruncate` zero-expansion & shrinking, path-based `truncate()`, and `getline()` dynamic streaming.
    - [x] `/test/path_test`: deep nested directory topology (5 levels), canonicalization of relative `.` and `..`, `chdir`/`getcwd`, and error semantics.
    - [x] `/test/storage_test`: SD card hardware detection, block node presence (`/dev/sd0p1`), and FatFs volume mounting/read/write.
    - [x] `/test/stress_test`: rapid thread creation/join churn, concurrent multi-thread mutex contention, FIFO message queues, and monotonic jitter verification.
    - [x] `/test/all`: sequential runner executing all 9 test suites and reporting aggregate pass/fail status.
    - [x] Verified **100% ALL PASSED (9/9 suites)** on hardware on PolarFire SoC Discovery Kit.
    - [x] Bound commands to linker sections (`FLX_BINFMT_CMD_SECTION` & `FLX_BINFMT_DECLARE_CMD_PAYLOAD`) for exact executable byte size reporting in `stat` and `ls -li`.
  - [x] Enhance `/bin/ls` with `-i` flag to display runtime memory address of executables/files (distinguishing LIM `0x08000000` vs DDR `0x80000000` execution).
  - [x] Rebalance unused hart stacks (Harts 2-4) in `mpfs-lim.ld` to guarantee 100% clean builds fitting into 512 KB `l2lim`.
  - [x] Build both `DDR-Debug-DiscoveryKit` and `LIM-Debug-DiscoveryKit` targets (0 errors, 0 warnings) and regenerate `payload.bin`.
- [x] **Phase 4: Layered SAL + POSIX Architecture Finalized**
  - [x] Preserved RTOS-agnostic Scheduler Abstraction Layer (SAL) (`sched_thread_*` in `felix/kernel/sched.h`) for core kernel daemons and minimal LIM/AMP targets.
  - [x] Exposed standard `FreeRTOS+POSIX` (`pthread`, `semaphore`, `mqueue`, `timer`) for userland applications, Toybox, and test suites with PCB TLS inheritance.

---

## 3. Configurability Subsystem (Kconfig)

Adopt the in-tree Kconfig system from `felix/external/toybox/kconfig` to generate compile-time profile headers (`autoconf.h`). (See [`docs/dynamic_loading_and_configurability_design.md`](file:///c:/git/felix-bsp-mpfs/docs/dynamic_loading_and_configurability_design.md)).

- [x] **Kconfig Tooling Integration**
  - [x] Create root `felix/Kconfig` and `felix/scripts/kconfig.py` defining memory models, loader options, and granular FreeRTOS+POSIX features.
  - [x] Add interactive terminal menu (`menuconfig`) and native Tk graphical editor (`guiconfig`) support.
- [x] **Granular Test Suite Configuration & Subsystem Dependency Gating**
  - [x] Create `menu "Verification & Test Suite"` in `felix/Kconfig` with master toggle `CONFIG_FELIX_TEST_SUITE` and runner toggle `CONFIG_FELIX_TEST_ALL`.
  - [x] Add granular toggles for all 9 suites with strict subsystem dependencies:
    - `FELIX_TEST_DEV`, `FELIX_TEST_VFS`, `FELIX_TEST_IO`, `FELIX_TEST_PATH` depend on `FELIX_VFS`.
    - `FELIX_TEST_POSIX` and `FELIX_TEST_STRESS` depend on `FELIX_FREERTOS_PLUS_POSIX`.
    - `FELIX_TEST_WORDEXP` depends on `FELIX_POSIX`.
    - `FELIX_TEST_STORAGE` depends on `FELIX_BLK_MANAGER`.
  - [x] Dynamic `/test/all` runner automatically discovering and executing only Kconfig-enabled suites.
  - [x] Stubbing architecture preserving CDT `subdir.mk` builds while stripping ~52 KB code when tests are disabled.
- [x] **Target Defconfigs**
  - [x] Create `disco_lim_defconfig` (monolithic, deterministic real-time, no loader).
  - [x] Create `disco_ddr_defconfig` (DDR memory, ELF loader, dynamic libraries, full POSIX).
  - [x] Create `disco_amp_defconfig` (Harts 1-3 LIM control, Hart 4 DDR supervisor).
- [x] **Header Generation**
  - [x] Validate automated generation of `felix/include/generated/autoconf.h`.

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

## 6. VFS, Storage & Peripheral Maturation (Completed)

- [x] **SD/eMMC Card Driver & Hotplugging**
  - [x] Re-enable `flx_blk_manager_init()` and `blk_manager_daemon_task` in `init_task()`.
  - [x] Verify MMC/SD interrupt handling and transfer rates on Discovery Kit.
- [x] **Filesystem Partitioning & Mounting**
  - [x] Verify MBR and 64-bit GPT partition table parsing on real SD card (`/dev/sd0p1`, `/dev/sd0p2`).
  - [x] Auto-mount FAT/exFAT partition to `/mnt/sd0p2`.
- [x] **Standard Dev Nodes**
  - [x] Verify `/dev/console` (major 4, minor 4) bidirectional UART operations (input + output).
  - [x] Implement & verify `/dev/null` (major 1, minor 3), `/dev/zero` (major 1, minor 5), and raw block device nodes (`/dev/sd0`, `/dev/sd0p1`, `/dev/sd0p2`).

---

## 7. Hardware Bench & Toolchain Testing (Completed)

- [x] **Flash & Run LIM Build on Board**
  - [x] Load `LIM-Debug-DiscoveryKit` binary via SoftConsole JTAG.
  - [x] Capture early UART boot logs and verify Felix shell prompt.
- [x] **Test DDR Build on Board**
  - [x] Load `DDR-Debug-DiscoveryKit` binary (JTAG live load and autonomous HSS SD boot).
  - [x] Confirmed E51 finishes DDR training and passes execution to Hart 1 at `0x80000000`.
  - [x] Confirmed PMP configuration and full 9/9 test suite execution in DDR.
- [x] **Headless Command-Line Build Scripts**
  - [x] Created CLI build & deployment helpers (`scripts/build.ps1`, `scripts/build.bat`, `scripts/generate_payload.ps1`, `scripts/update_payload.ps1`, `scripts/prepare_sd.ps1`).
