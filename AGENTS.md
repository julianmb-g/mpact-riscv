# mpact-riscv Agent Instructions

## Lessons Learned

### Build Dependencies
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
- **SPSC Buffer State Export**: The `AsyncFormattingDaemon` must organically evaluate trace synchronization mechanisms by exporting the drained state of the SPSC Ring Buffer (`g_rvvi_trace_buffer_drained`). Downstream targets (like Campaign Runner) use this flag to explicitly throw `std::runtime_error("SPSC buffer drain timeout")` when checking for backpressure blockages.

### Diagnostic Artifact Preservation
- **Orphaned Scripts**: Diagnostic scripts like `diff.txt` must be securely preserved via `.gitignore` rather than blindly deleted, ensuring CI traceability and clean orchestration pointer updates.

### absl::StatusOr Pointer Unwrapping Ban Remediation
- **Safe Unwrapping**: When resolving `absl::StatusOr` unwrap violations in `mpact-riscv` (e.g., `GetCsr` calls in `riscv_top.cc` or `riscv_priv_instructions.cc`), ensure all pointer unwraps (`*res`, `*result`) are replaced with explicit `.value()` calls (e.g., `res.value()`). This enforces strict memory safety boundaries and complies with the architectural pointer unwrapping ban.


### SPSC Ring Buffer Concurrency
- **SPSC Ring Buffer Concurrency & Backpressure**: Implement strict lock-free SPSC backpressure boundaries. If the ring buffer fills, the producer must explicitly yield (`absl::SleepFor(absl::Milliseconds(1))`, not `sched_yield()`). Evaluate a monotonic clock against a strict timeout (e.g., `kSpscYieldTimeoutMs = 5000`) and throw `std::runtime_error` if the buffer fails to drain. Validate this organically in test beds via `EXPECT_THROW` rather than masking wall-clock deadlocks.
