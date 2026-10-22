# FreeRTOS+POSIX Integration Plan & Checklist

## 1. Executive Summary & Architecture

Felix OS currently features a lean, custom POSIX compatibility layer located in [`felix/posix/`](../felix/posix/) and [`felix/kernel/syscalls.c`](../felix/kernel/syscalls.c). This layer specifically implements:
* **Virtual File System (VFS)**: File descriptors, path routing, and standard file operations (`open`, `read`, `write`, `close`, `lseek`, `truncate`).
* **POSIX.1-2008 `*at` Shims**: Directory-relative calls (`openat`, `fstatat`, `unlinkat`, `mkdirat`, `fchmodat`, `fchownat`, `utimensat`) with `AT_FDCWD` support.
* **Directory Streams**: `opendir`, `readdir`, `closedir`, `scandir`.
* **Terminal & Devices**: `termios`, `tcgetattr`, `tcsetattr`, `ioctl`, `cfmakeraw`.
* **OS Identity & Metadata**: `stat`, `mount`, `umount`, `statfs`, `getenv`, `setenv`, `getuid`, `uname`.

### The Missing Half: Concurrency & IPC
Felix OS currently lacks standard user-space POSIX execution and synchronization primitives (`pthread_*`, `sem_*`, `mq_*`). The FreeRTOS+POSIX library (`FreeRTOS-Plus-POSIX`) provides exactly these missing pieces while offering zero filesystem/VFS capabilities.

By combining them, Felix OS achieves full POSIX coverage:
```
+-----------------------------------------------------------------------------------+
|                                 Felix OS POSIX API                                |
+-----------------------------------------+-----------------------------------------+
|        Filesystem, Devices & OS         |            Execution & IPC              |
|        [felix/posix + syscalls.c]       |          [FreeRTOS-Plus-POSIX]          |
+-----------------------------------------+-----------------------------------------+
| - VFS & File Descriptors                | - POSIX Threads (pthread_create/join)   |
| - open, read, write, close, lseek       | - Mutexes, Condvars, RWLocks, Barriers  |
| - POSIX.1-2008 *at operations           | - POSIX Semaphores (sem_init/wait/post) |
| - Directory Streams (opendir/readdir)   | - POSIX Message Queues (mq_open/send)   |
| - Terminal Control (termios / ioctl)    | - POSIX Timers (timer_create/settime)   |
| - Mounts, StatFS, Environment Variables | - POSIX Scheduler Policies (sched_*)    |
+-----------------------------------------+-----------------------------------------+
|             Felix VFS / Kernel          |             FreeRTOS Kernel             |
+-----------------------------------------+-----------------------------------------+
```

---

## 2. Integration Checklist

### Phase 1: Dependency Ingestion & Scope Isolation
- [x] **1.1 Source submodule or bundle**: Add `FreeRTOS-Plus-POSIX` repository as a submodule (`felix/external/FreeRTOS-Plus-POSIX`).
- [x] **1.2 Verify FreeRTOS-Kernel compatibility**: Verified `FreeRTOS-Plus-POSIX` compatibility with `felix/external/FreeRTOS-Kernel` (V10.4.x) and configured `configUSE_APPLICATION_TASK_TAG`, `configSUPPORT_STATIC_ALLOCATION`, and `configKERNEL_PROVIDED_STATIC_MEMORY` in [`felix/osal/freertos/FreeRTOSConfig.h`](../felix/osal/freertos/FreeRTOSConfig.h).
- [x] **1.3 Define required feature set (Kconfig configurable)**:
  - `CONFIG_FELIX_FREERTOS_PLUS_POSIX` (master toggle in [`felix/Kconfig`](../felix/Kconfig))
  - `CONFIG_FELIX_POSIX_PTHREAD` (`pthread` threads, mutexes, condition variables, barriers)
  - `CONFIG_FELIX_POSIX_SEMAPHORE` (`sem_init`, `sem_wait`, `sem_post`, `sem_timedwait`, `sem_trywait`)
  - `CONFIG_FELIX_POSIX_MQUEUE` (`mq_open`, `mq_send`, `mq_receive`, `mq_unlink`)
  - `CONFIG_FELIX_POSIX_TIMER` (`timer_create`, `timer_settime`, `timer_gettime`, `timer_delete`)

---

### Phase 2: Header Collision Prevention & `#include_next` Integrity
Felix headers in `felix/include/` intercept standard headers (e.g. `time.h`, `unistd.h`, `signal.h`, `sys/types.h`) and use GCC's `#include_next` to wrap Newlib. A naive global include of FreeRTOS+POSIX breaks this chain.

- [x] **2.1 Isolate FreeRTOS+POSIX include path**:
  - Added thin Kconfig-gated public wrapper headers in `felix/include/`:
    - [`felix/include/pthread.h`](../felix/include/pthread.h)
    - [`felix/include/semaphore.h`](../felix/include/semaphore.h)
    - [`felix/include/mqueue.h`](../felix/include/mqueue.h)
    - [`felix/include/sched.h`](../felix/include/sched.h)
- [x] **2.2 Prevent `<time.h>` shadowing**:
  - Kept [`felix/include/time.h`](../felix/include/time.h) as the primary interceptor chaining to Newlib via `#include_next <time.h>`.
  - Added POSIX.1b `struct itimerspec` and `timer_create`/`timer_settime`/`timer_gettime`/`timer_delete` declarations under `CONFIG_FELIX_POSIX_TIMER`.
- [x] **2.3 Prevent `<unistd.h>`, `<fcntl.h>`, `<errno.h>`, & `<signal.h>` duplication**:
  - Kept Felix's [`felix/include/unistd.h`](../felix/include/unistd.h) and [`felix/include/signal.h`](../felix/include/signal.h) authoritative.
  - Pre-included Newlib's `<errno.h>`, `<fcntl.h>`, and `<signal.h>` in [`felix/osal/freertos/FreeRTOS_POSIX_portable.h`](../felix/osal/freertos/FreeRTOS_POSIX_portable.h) and defined `_FREERTOS_POSIX_ERRNO_H_`, `_FREERTOS_POSIX_FCNTL_H_`, and `_FREERTOS_POSIX_SIGNAL_H_` so `FreeRTOS-Plus-POSIX` uses Newlib's native `errno` and `O_*` flags directly.
- [x] **2.4 Resolve Newlib 32-bit `<sys/_pthreadtypes.h>` truncation on 64-bit RISC-V (`sys/types.h`)**:
  - Newlib's `<sys/_pthreadtypes.h>` defines `pthread_t`, `pthread_mutex_t`, and `pthread_cond_t` as 32-bit `__uint32_t` (4 bytes), which truncates 64-bit DDR pointers (`0x8000xxxx`) on RV64 and corrupts the stack when `FreeRTOS-Plus-POSIX` writes internal structs.
  - Suppressed `<sys/_pthreadtypes.h>` (`#define _SYS__PTHREADTYPES_H_`) and `<sys/sched.h>` (`#define _SYS_SCHED_H_`) in [`felix/include/sys/types.h`](../felix/include/sys/types.h), defined 64-bit `typedef void *pthread_t;`, `typedef void *pthread_attr_t;`, and `typedef void *timer_t;`, and enabled full struct definitions (`PthreadMutexType_t`, `PthreadCondType_t`, `PthreadBarrierType_t`) in [`felix/osal/freertos/FreeRTOS_POSIX_portable.h`](../felix/osal/freertos/FreeRTOS_POSIX_portable.h).

---

### Phase 3: Source File Filtering & Linker Collision Resolution
Prevent duplicate symbol errors between Felix POSIX C files and FreeRTOS+POSIX C files.

- [x] **3.1 Exclude conflicting FreeRTOS+POSIX C sources via single translation-unit bridge [`felix/osal/freertos/freertos_posix.c`](../felix/osal/freertos/freertos_posix.c)**:
  - ❌ **Excluded** `FreeRTOS_POSIX_clock.c` (Felix's [`posix/time.c`](../felix/posix/time.c) implements `clock_gettime` and `nanosleep`; helper routines `UTILS_AbsoluteTimespecToDeltaTicks`, `UTILS_TimespecToTicks`, `UTILS_TimespecAdd`, `UTILS_TimespecCompare`, `UTILS_ValidateTimespec` provided in `freertos_posix.c`).
  - ❌ **Excluded** `FreeRTOS_POSIX_unistd.c` (Felix's [`posix/unistd.c`](../felix/posix/unistd.c) implements `sleep` and `usleep`).
- [x] **3.2 Include desired FreeRTOS+POSIX C sources (gated by Kconfig in `freertos_posix.c`)**:
  - [x] `FreeRTOS_POSIX_utils.c` (`CONFIG_FELIX_FREERTOS_PLUS_POSIX`)
  - [x] `FreeRTOS_POSIX_sched.c` (`CONFIG_FELIX_POSIX_PTHREAD`)
  - [x] `FreeRTOS_POSIX_pthread.c` (`CONFIG_FELIX_POSIX_PTHREAD`)
  - [x] `FreeRTOS_POSIX_pthread_mutex.c` (`CONFIG_FELIX_POSIX_PTHREAD`)
  - [x] `FreeRTOS_POSIX_pthread_cond.c` (`CONFIG_FELIX_POSIX_PTHREAD`)
  - [x] `FreeRTOS_POSIX_pthread_barrier.c` (`CONFIG_FELIX_POSIX_PTHREAD`)
  - [x] `FreeRTOS_POSIX_semaphore.c` (`CONFIG_FELIX_POSIX_SEMAPHORE`)
  - [x] `FreeRTOS_POSIX_mqueue.c` (`CONFIG_FELIX_POSIX_MQUEUE`)
  - [x] `FreeRTOS_POSIX_timer.c` (`CONFIG_FELIX_POSIX_TIMER`)
- [x] **3.3 Reconcile `sched_yield()` signature & Process PCB Inheritance**:
  - Updated [`felix/kernel/sched.h`](../felix/kernel/sched.h) so `flx_sched_yield()` provides the inline kernel helper and `int sched_yield(void)` matches POSIX when `CONFIG_FELIX_POSIX_PTHREAD` is enabled.
  - Added `traceTASK_CREATE(pxNewTCB)` in [`felix/osal/freertos/FreeRTOSConfig.h`](../felix/osal/freertos/FreeRTOSConfig.h) so threads spawned via `pthread_create()` automatically inherit their parent task's `FlxProcPCB_t *` in FreeRTOS TLS slot 0.

---

### Phase 4: Build System Configuration (.cproject & Makefiles)
- [x] **4.1 Eclipse CDT configuration**:
  - Updated [`.cproject`](../.cproject) (`DDR-Debug-DiscoveryKit` and `LIM-Debug-DiscoveryKit`) with `FreeRTOS-Plus-POSIX` private/portable include paths.
  - Kept `external/FreeRTOS-Plus-POSIX` excluded from direct CDT directory scanning so `osal/freertos/freertos_posix.c` cleanly controls compilation via Kconfig.
- [x] **4.2 Memory budgeting verification**:
  - Verified both `DDR-Debug-DiscoveryKit` (`text = 222,740 B`) and `LIM-Debug-DiscoveryKit` (`text = 222,608 B`, `bss = 123,808 B`) build with 0 errors and fit within their linker regions.

---

### Phase 5: Verification & Test Suite (`/test/posix_test`)
- [x] **5.1 Thread Creation & Join**: Implemented and verified `pthread_create()` spawning worker tasks and `pthread_join()` collecting return values in [`felix/fs/rootfs/test/flx_fs_test.c`](../felix/fs/rootfs/test/flx_fs_test.c).
- [x] **5.2 Synchronization**: Verified `pthread_mutex_lock()` / `unlock()` protecting a shared counter across concurrent worker pthreads.
- [x] **5.3 Semaphores**: Verified `sem_init()`, `sem_wait()`, `sem_post()`, and `sem_destroy()` synchronization between pthreads.
- [x] **5.4 VFS Coexistence**: Verified a `pthread` worker inherits the process PCB (`traceTASK_CREATE`) and performs Felix VFS operations (`open`, `write`, `read`, `close`, `unlink`) on `/tmp/posix_pthread_vfs.txt`.
- [x] **5.5 Condition Variables & Barriers**: Verified `pthread_cond_wait()` / `pthread_cond_signal()` producer-consumer coordination and `pthread_barrier_wait()` multi-thread synchronization with `PTHREAD_BARRIER_SERIAL_THREAD` winner detection.
- [x] **5.6 Message Queues & Timers**: Verified `mq_open()`, `mq_send()`, `mq_receive()`, `mq_close()`, and `mq_unlink()` message exchange, and `timer_create()` / `timer_settime()` (`SIGEV_THREAD`) dispatching high-resolution timer callbacks waking a semaphore.
- [x] **5.7 DRY Unit Test Framework & Suite Expansion**: Created [`felix/include/flx_test.h`](../felix/include/flx_test.h) with `FlxTestCtx_t`, relational comparisons (`"=="`, `"!="`, `"<"`, `"<="`, `">"`, `">="`), call-site tracking (`__FILE__`, `__func__`, `__LINE__`), and standardized CLI arguments (`-v`, `-F`, `-q`, `-h`). Implemented `/test/mem_test`, `/test/dev_test`, `/test/vfs_test` (with `-d <dir>`), and aggregate runner `/test/all`.
- [x] **5.8 Section Sizing & Memory Address Introspection**: Annotated all `/test/*` commands with `FLX_BINFMT_CMD_SECTION` and `FLX_BINFMT_DECLARE_CMD_PAYLOAD` so `stat` and `ls -li` report exact compiled byte sizes and memory addresses. Added `-i` option to `/bin/ls` to display executable memory addresses to verify LIM (`0x08000000`) vs DDR (`0x80000000`) execution.
