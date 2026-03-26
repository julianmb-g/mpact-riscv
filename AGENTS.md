# mpact-riscv Agent Instructions
## Lessons Learned

### Architecture Quirks
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.

### C++ & System Programming
- **absl::StatusOr Pointer Unwrapping Ban**: Never unwrap `absl::StatusOr<T>` using the `*` pointer operator (e.g., `*res`). Always explicitly call `.value()` after checking `.ok()` to comply with strict memory safety boundaries. When resolving `absl::StatusOr` unwrap violations in C++ (e.g., `GetCsr` calls), ensure all pointer unwraps (`*res`, `*result`) are replaced with explicit `.value()` calls (e.g., `res.value()`). This enforces strict memory safety boundaries and complies with the architectural pointer unwrapping ban. When resolving `absl::StatusOr` unwrap violations in `mpact-riscv` (e.g., `GetCsr` calls in `riscv_top.cc` or `riscv_priv_instructions.cc`), ensure all pointer unwraps (`*res`, `*result`) are replaced with explicit `.value()` calls (e.g., `res.value()`)

### Miscellaneous
- **CsrDirtyList Implementation**: Successfully integrated `CsrDirtyList` into `mpact-riscv/riscv/riscv_csr.h` to track modified CSR addresses.
- **RISC-V RMM Rounding Mode**: When implementing operations that respect the `RMM` (Round to Nearest, ties to Max Magnitude) rounding mode, remember that `ScopedFPStatus` translates it to standard ties-to-even on most hosts. For operations that natively implement rounding (like `fround`), you must manually intercept `kRoundToNearestTiesToMax` and apply `std::round` (which intrinsically rounds halves away from zero) to ensure correct architectural fidelity.

- **Floating-Point Boundaries**: Avoid hardcoding raw float maximums directly in inline code logic. Assign them explicitly to named `constexpr`/`const` constants (e.g., `constexpr double kTwoPow32 = 4294967296.0;`). Avoid using `std::pow` as it is not `constexpr` and introduces hot-path overhead.
