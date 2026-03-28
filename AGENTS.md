# mpact-riscv Agent Instructions

## Lessons Learned

### Build & Orchestration
- **Local Repository Prohibition**: Replacing `http_archive` with `local_repository` or `native.local_repository` in Bazel repository definitions (e.g., `repos.bzl`) is strictly forbidden across all submodules. Doing so breaks hermeticity and cross-system reproducibility.

### Architecture Quirks
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.

### C++ & System Programming
- **absl::StatusOr Pointer Unwrapping Ban**: Never unwrap `absl::StatusOr<T>` using the `*` pointer operator (e.g., `*res`). Always explicitly call `.value()` after checking `.ok()` to comply with strict memory safety boundaries. When resolving `absl::StatusOr` unwrap violations in C++ (e.g., `GetCsr` calls), ensure all pointer unwraps (`*res`, `*result`) are replaced with explicit `.value()` calls (e.g., `res.value()`). This enforces strict memory safety boundaries and complies with the architectural pointer unwrapping ban. When resolving `absl::StatusOr` unwrap violations in `mpact-riscv` (e.g., `GetCsr` calls in `riscv_top.cc` or `riscv_priv_instructions.cc`), ensure all pointer unwraps (`*res`, `*result`) are replaced with explicit `.value()` calls (e.g., `res.value()`)

### Miscellaneous
- **Authentic Execution Boundaries**: When adding new instruction sets (e.g., Zfa), do not rely solely on unit tests instantiating raw `generic::Instruction` objects with explicitly mapped operands. There must be E2E integration coverage ensuring authentic, cross-compiled ELFs execute securely within the broader top-level simulators (e.g., `rv64g_sim`), validating the boundary between instruction traps and the execution loop.
- **CsrDirtyList Implementation**: Successfully integrated `CsrDirtyList` into `mpact-riscv/riscv/riscv_csr.h` to track modified CSR addresses.
- **Floating-Point Boundaries**: Avoid hardcoding raw float maximums directly in inline code logic. Assign them explicitly to named `constexpr`/`const` constants (e.g., `constexpr double kTwoPow32 = 4294967296.0;`). Avoid using `std::pow` as it is not `constexpr` and introduces hot-path overhead.
- **Mathematical Trace Fidelity vs Cosmetic String Assertions**: When validating hardware tracing APIs (like `rvvi_trace_event_t`), it is strictly forbidden to use cosmetic evaluation limits (e.g. `EXPECT_EQ(output, "PASS")`). Out-of-band tracing mechanisms must be validated by implementing explicit temporal limits (like chronological timestamps and monotonic commit tracking) and mathematically accumulating the structural deltas to natively re-derive the hardware state. (e.g., `EXPECT_EQ(recomputed_x5_state, 50)`).
- **RISC-V RMM Rounding Mode**: When implementing operations that respect the `RMM` (Round to Nearest, ties to Max Magnitude) rounding mode, remember that `ScopedFPStatus` translates it to standard ties-to-even on most hosts. For operations that natively implement rounding (like `fround`), you must manually intercept `kRoundToNearestTiesToMax` and apply `std::round` (which intrinsically rounds halves away from zero) to ensure correct architectural fidelity.
- **RiscvDtbLoader Boot Sequence Integrity**: Tests validating the OpenSBI hardware handshake (e.g. `riscv_dtb_loader_test`) must utilize mock/dummy payloads and enforce organic failure via `absl::IsNotFound` when required artifacts are completely missing, rather than skipping the test with `GTEST_SKIP()`. This exposes configuration drift natively.

### ABI Struct Redefinition
- **Hardware Interrupt Testing Authenticity**: When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually. Instead, they must instantiate the full `RiscVTop` CPU execution loop and verify that the interrupt organically traps and updates `epc` to the correct execution offset. Use `NativeTextualAssembler` to dynamically compile authentic RISC-V payloads (like `wfi`) to guarantee true E2E routing and avoid mock boundaries.
- **Struct Definition Duplication Across Submodules**: When multiple repositories define the exact same struct (e.g., `rvvi_trace_event_t`) for ABI plugin compliance, they must wrap the definition in `#ifndef` guards (e.g., `#ifndef RVVI_TRACE_EVENT_T_DEFINED`) to prevent fatal C++ `typedef redefinition` errors during cross-repository compilation and linking.
- **Zfa E2E Execution Trapping**: When validating new instruction extensions like Zfa (`fround.s`, `fcvtmod.w.d`), the execution sequence must be natively decoded by the top-level simulator loop (`RiscVTop::Step()`) using explicitly encoded bytes (e.g., `0x4045c553` for `fround.s`) to organically prove that the instruction traps and routes securely without isolating or mocking the instruction context boundaries.

- **Zve32f E2E Execution Trapping**: When validating new instruction extensions like `Zve32f` (`vfsqrt.v`), the execution sequence must be natively decoded by the top-level simulator loop (`RiscVTop::Step()`) using explicitly encoded bytes (e.g., `0x4e2010d7` for `vfsqrt.v` and `0xcd0272d7` for `vsetivli`) to organically prove that the instruction traps and routes securely without isolating or mocking the instruction context boundaries.

## Local Submodule Lessons
\n## QA Lessons (Current Cycle)\n- **Control Flow Integrity & CSR Execution (`Zicfiss`/`Smstateen`):** When implementing execution logic for new extensions, tests that exclusively evaluate raw `generic::Instruction` objects with explicitly mapped operands constitute a mocked boundary. All execution pathways must be proven by routing an authentic, cross-compiled ELF through the `RiscVTop` instruction decoder.

### Missing Dependencies in BUILD files
- **Bazel Header Resolution (`elf_program_loader.h`)**: When including headers from `mpact-sim` (like `mpact/sim/util/program_loader/elf_program_loader.h`), the corresponding Bazel `cc_library` target in `mpact-riscv` MUST explicitly include the exact dependency target name (e.g., `@com_google_mpact_sim//mpact/sim/util/program_loader:elf_loader`) in its `deps` list. Failing to do so results in fatal "file not found" compilation errors.

### New QA Lessons (Current Cycle)
- **Control Flow Integrity & CSR Execution (`Zicfiss`/`Smstateen`):** When implementing execution logic for new extensions, tests that exclusively evaluate raw `generic::Instruction` objects with explicitly mapped operands constitute a mocked boundary. All execution pathways must be proven by routing an authentic, cross-compiled ELF through the `RiscVTop` instruction decoder.
