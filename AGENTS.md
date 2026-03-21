# mpact-riscv Agent Instructions

## Lessons Learned


### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Build Dependencies
- **Bazel Target Hallucination & Broken Dependencies**: Do not use `@@com_google_mpact-riscv//riscv:rvvi_sim` as a dependency. Use `@com_google_mpact-riscv//riscv:rv32g_sim` or `rv64g_sim`. Referencing non-existent upstream targets (like `rvvi_sim` instead of `rv32g_sim` or `rv64g_sim`) violently crashes downstream builds during analysis.
- **CLI Tracing Flags**: When integrating trace hooks like `--rvvi_trace` and `--log_commits` into top-level binaries (`rv64g_sim.cc`, `rv32g_sim.cc`), `AsyncFormattingDaemon` and `RvviMemoryMapper` must be explicitly initialized prior to the execution run to ensure SPSC buffer backpressure is natively hooked during compilation.
