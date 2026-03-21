# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Build Dependencies
- **Bazel Target Hallucination**: Do not use `@@com_google_mpact-riscv//riscv:rvvi_sim` as a dependency. Use `@com_google_mpact-riscv//riscv:rv32g_sim` or `rv64g_sim`.
- **Broken Bazel Dependency Management**: Referencing non-existent upstream targets (like `rvvi_sim` instead of `rv32g_sim` or `rv64g_sim`) violently crashes downstream builds during analysis.

