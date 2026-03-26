# mpact-riscv Agent Instructions
## Lessons Learned

### Miscellaneous
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
### CsrDirtyList Implementation\n- **Event-Driven Structs**: Added `CsrDirtyList` struct to `riscv_csr.h` to handle event-driven tracking of modified CSRs.
- **RISC-V RMM Rounding Mode**: When implementing operations that respect the `RMM` (Round to Nearest, ties to Max Magnitude) rounding mode, remember that `ScopedFPStatus` translates it to standard ties-to-even on most hosts. For operations that natively implement rounding (like `fround`), you must manually intercept `kRoundToNearestTiesToMax` and apply `std::round` (which intrinsically rounds halves away from zero) to ensure correct architectural fidelity.
