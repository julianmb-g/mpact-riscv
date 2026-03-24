# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **RVVI Struct ABI Alignment**: The `rvvi_trace_event_t` POD struct must be locked to exactly 64-bytes. Do not hallucinate a 128-byte cache-line alignment which causes fatal cross-compilation misalignments.

### Build Dependencies
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
