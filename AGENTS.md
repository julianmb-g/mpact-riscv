# mpact-riscv Agent Instructions

## Lessons Learned

### Build Dependencies
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
- **SPSC Buffer State Export**: The `AsyncFormattingDaemon` must organically evaluate trace synchronization mechanisms by exporting the drained state of the SPSC Ring Buffer (`g_rvvi_trace_buffer_drained`). Downstream targets (like Campaign Runner) use this flag to explicitly throw `std::runtime_error("SPSC buffer drain timeout")` when checking for backpressure blockages.

### Diagnostic Artifact Preservation
- **Orphaned Scripts**: Diagnostic scripts like `diff.txt` must be securely preserved via `.gitignore` rather than blindly deleted, ensuring CI traceability and clean orchestration pointer updates.
