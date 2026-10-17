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
- [ ] **1.1 Source submodule or bundle**: Add `FreeRTOS-Plus-POSIX` repository as a submodule (or copy into `felix/external/FreeRTOS-Plus-POSIX`).
- [ ] **1.2 Verify FreeRTOS-Kernel compatibility**: Ensure the FreeRTOS-Plus-POSIX version aligns with `felix/external/FreeRTOS-Kernel` (V10.4.x).
- [ ] **1.3 Define required feature set**:
  - `pthread` (threads, mutexes, condition variables, reader-writer locks, barriers, thread keys)
  - `semaphore` (counting semaphores: `sem_init`, `sem_wait`, `sem_post`)
  - `mqueue` (message queues: `mq_open`, `mq_send`, `mq_receive`)
  - `timer` (interval timers: `timer_create`, `timer_settime`)

---

### Phase 2: Header Collision Prevention & `#include_next` Integrity
Felix headers in `felix/include/` intercept standard headers (e.g. `time.h`, `unistd.h`, `signal.h`, `sys/types.h`) and use GCC's `#include_next` to wrap Newlib. A naive global include of FreeRTOS+POSIX breaks this chain.

- [ ] **2.1 Isolate FreeRTOS+POSIX include path**:
  - Do **NOT** add the FreeRTOS+POSIX root `include/` directory directly to global `-I`.
  - Expose only non-colliding headers to the global search path:
    - `<pthread.h>`
    - `<semaphore.h>`
    - `<mqueue.h>`
- [ ] **2.2 Prevent `<time.h>` shadowing**:
  - Ensure Felix's [`felix/include/time.h`](../felix/include/time.h) remains the primary interceptor chaining to Newlib.
  - Expose FreeRTOS+POSIX timer declarations (`timer_create`, `timer_settime`, `struct itimerspec`) without replacing Newlib's `struct tm`, `strftime`, `localtime`.
- [ ] **2.3 Prevent `<unistd.h>` & `<signal.h>` duplication**:
  - Keep Felix's [`felix/include/unistd.h`](../felix/include/unistd.h) and [`felix/include/signal.h`](../felix/include/signal.h) authoritative.
  - Suppress FreeRTOS+POSIX's duplicate declarations for `sleep` and `usleep`.
- [ ] **2.4 Resolve Newlib `pthread_t` typedef collisions (`sys/types.h`)**:
  - Configure `FreeRTOS_POSIX_portable.h` to recognize Newlib's pre-existing types in `<sys/_pthreadtypes.h>`:
    ```c
    #define posixconfigENABLE_PTHREAD_T              0
    #define posixconfigENABLE_PTHREAD_ATTR_T         0
    #define posixconfigENABLE_PTHREAD_MUTEX_T        0
    #define posixconfigENABLE_PTHREAD_MUTEXATTR_T    0
    #define posixconfigENABLE_PTHREAD_COND_T         0
    #define posixconfigENABLE_PTHREAD_CONDATTR_T     0
    #define posixconfigENABLE_PTHREAD_RWLOCK_T       0
    #define posixconfigENABLE_PTHREAD_RWLOCKATTR_T   0
    #define posixconfigENABLE_TIMESPEC               0
    #define posixconfigENABLE_CLOCKID_T              0
    ```
  - Verify GCC compiles without `redefinition of typedef 'pthread_t'` errors.

---

### Phase 3: Source File Filtering & Linker Collision Resolution
Prevent duplicate symbol errors between Felix POSIX C files and FreeRTOS+POSIX C files.

- [ ] **3.1 Exclude conflicting FreeRTOS+POSIX C sources**:
  - ❌ **Exclude** `FreeRTOS_POSIX_clock.c` (Felix's [`posix/time.c`](../felix/posix/time.c) already implements `clock_gettime` and `nanosleep`).
  - ❌ **Exclude** `FreeRTOS_POSIX_unistd.c` (Felix's [`posix/unistd.c`](../felix/posix/unistd.c) already implements `sleep` and `usleep`).
  - ❌ **Exclude** any dummy/stub VFS or filesystem sources in FreeRTOS+POSIX.
- [ ] **3.2 Include desired FreeRTOS+POSIX C sources**:
  -  **Include** `FreeRTOS_POSIX_pthread.c`
  -  **Include** `FreeRTOS_POSIX_pthread_attr.c`
  -  **Include** `FreeRTOS_POSIX_pthread_mutex.c`
  -  **Include** `FreeRTOS_POSIX_pthread_cond.c`
  -  **Include** `FreeRTOS_POSIX_pthread_rwlock.c`
  -  **Include** `FreeRTOS_POSIX_pthread_barrier.c`
  -  **Include** `FreeRTOS_POSIX_semaphore.c`
  -  **Include** `FreeRTOS_POSIX_mqueue.c`
  -  **Include** `FreeRTOS_POSIX_timer.c` (interval timers)
  -  **Include** `FreeRTOS_POSIX_sched.c` (scheduler policies)
- [ ] **3.3 Reconcile `sched_yield()` signature**:
  - In [`felix/kernel/sched.h`](../felix/kernel/sched.h), Felix declares `static inline void sched_yield(void)`.
  - In POSIX, the signature is `int sched_yield(void)`.
  - Reconcile `sched.h` to return `0` to align cleanly with the POSIX standard.

---

### Phase 4: Build System Configuration (.cproject & Makefiles)
- [ ] **4.1 Eclipse CDT configuration**:
  - Add `FreeRTOS-Plus-POSIX/source` to Eclipse source entries with the exclusions defined in Phase 3.
  - Add necessary include directories to compiler options.
- [ ] **4.2 Memory budgeting verification**:
  - Check footprint impact on **LIM-Debug** (< 10–15 KB flash increase for `pthread` + `sem`).
  - Confirm heap/stack usage in FreeRTOS `FreeRTOSConfig.h`.

---

### Phase 5: Verification & Test Suite
- [ ] **5.1 Thread Creation & Join**: Test `pthread_create()` spawning a worker task and `pthread_join()` waiting for completion.
- [ ] **5.2 Synchronization**: Test `pthread_mutex_lock()` / `unlock()` protecting a shared counter across multiple harts/threads.
- [ ] **5.3 Semaphores**: Test `sem_wait()` and `sem_post()` synchronization between tasks.
- [ ] **5.4 VFS Coexistence**: Verify a `pthread` worker can safely perform Felix VFS operations (`open`, `write`, `read`, `close`) using standard file descriptors.
