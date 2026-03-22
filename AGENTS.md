# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Build Dependencies
- **Bazel Target Hallucination & Broken Dependencies**: Do not use `@@com_google_mpact-riscv//riscv:rvvi_sim` as a dependency. Use `@com_google_mpact-riscv//riscv:rv32g_sim` or `rv64g_sim`. Referencing non-existent upstream targets (like `rvvi_sim` instead of `rv32g_sim` or `rv64g_sim`) violently crashes downstream builds during analysis.
- **CLI Tracing Flags**: When integrating trace hooks like `--rvvi_trace` and `--log_commits` into top-level binaries (`rv64g_sim.cc`, `rv32g_sim.cc`), `AsyncFormattingDaemon` and `RvviMemoryMapper` must be explicitly initialized prior to the execution run to ensure SPSC buffer backpressure is natively hooked during compilation.
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
- **Orphaned Build Configuration**: Automated formatting tools (like buildifier) frequently alter Bazel BUILD file formatting alongside C++ modifications. Always ensure these configuration updates are explicitly staged and committed to prevent orphaned files drifting in the git stash.

### macOS Platform Bleed & Missing Backend Prevention
- **Memory Interface Initialization Order**: When injecting memory wrappers (like `MemoryWatcher`) into the simulation stack, the base `MemoryInterface` pointer MUST be updated before subsequent architectural structures (like `AtomicMemory` or `RvviMemoryMapper`) are initialized. Failing to order the instantiation correctly will result in unmapped interfaces and fatal segmentation faults during execution.
- **SPSC Ring Buffer Thread Concurrency**: When enforcing backpressure in `SpscRingBuffer`, do not rely on `std::this_thread::yield()`. Use robust OS-level thread yielding such as `sched_yield()` or bounded `absl::SleepFor(absl::Milliseconds(1))` to ensure the ISS producer thread completely relinquishes control to the consumer thread when the buffer is full, preventing CPU starvation and 1500s Watchdog test timeouts. Additionally, use `std::chrono` to implement a bounded timeout (e.g., 10 seconds) within the SPSC yield loop to proactively detect deadlocks and throw `std::runtime_error("RVVI Trace SPSC Buffer Deadlock")` instead of yielding infinitely.
- **Thread Lifecycle Teardown Integrity**: When spawning detached background threads (like `AsyncFormattingDaemon`), they must be explicitly joined or stopped (e.g., `rvvi_daemon->Stop()`) before the main thread exits to prevent violent `std::terminate()` exceptions upon process shutdown.
- **Top-Level Simulator Tracing hooks**: Exposing architecture simulation traces via native CLI flags (`--rvvi_trace`, `--log_commits`) prevents the need for brittle recompilation or downstream proxy hacks. Natively bind `AsyncFormattingDaemon` initialization directly inside `rv32g_sim`/`rv64g_sim` before executing `riscv_top.Run()` to organically track state mutations.
