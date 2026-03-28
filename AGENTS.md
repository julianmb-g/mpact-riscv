# mpact-riscv Agent Instructions

## Lessons Learned

### Tier 1: Critical Architecture, Testing Boundaries & Build Integrity

- **Authentic Execution Boundaries & Hardware Trapping (Zfa/Zve32f/Zicfiss/Smstateen)**
  - **Quote:** "When adding new instruction sets... do not rely solely on unit tests instantiating raw `generic::Instruction` objects with explicitly mapped operands. All execution pathways must be proven by routing an authentic, cross-compiled ELF through the `RiscVTop` instruction decoder."
  - **Impact:** Mocking instruction context boundaries masks E2E routing failures and cross-component architectural trapping logic.
  - **Action:** Natively decode execution sequences via the top-level simulator loop (`RiscVTop::Step()`) using cross-compiled ELFs or explicitly encoded bytes to organically prove that the instructions trap and route securely.

- **Hardware Interrupt Testing Authenticity**
  - **Quote:** "When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually."
  - **Impact:** Manual evaluation creates mock boundaries that fail to prove actual execution trapping.
  - **Action:** Instantiate the full `RiscVTop` CPU execution loop, use `NativeTextualAssembler` to dynamically compile authentic RISC-V payloads (like `wfi`), and verify organic traps and correct `epc` updates.

- **Mathematical Trace Fidelity vs Cosmetic String Assertions**
  - **Quote:** "When validating hardware tracing APIs (like `rvvi_trace_event_t`), it is strictly forbidden to use cosmetic evaluation limits (e.g. `EXPECT_EQ(output, "PASS")`)."
  - **Impact:** Cosmetic assertions fail to guarantee the structural state correctness of the hardware.
  - **Action:** Implement explicit temporal limits (like chronological timestamps and monotonic commit tracking) and mathematically accumulate the structural deltas to natively re-derive the hardware state.

- **RiscvDtbLoader Boot Sequence Integrity**
  - **Quote:** "Tests validating the OpenSBI hardware handshake (e.g. `riscv_dtb_loader_test`) must utilize mock/dummy payloads and enforce organic failure via `absl::IsNotFound` when required artifacts are completely missing, rather than skipping the test with `GTEST_SKIP()`."
  - **Impact:** Using `GTEST_SKIP()` mathematically masks configuration drift and missing boot artifacts natively.
  - **Action:** Enforce organic failure natively when artifacts are missing.

- **Local Repository Prohibition**
  - **Quote:** "Replacing `http_archive` with `local_repository` or `native.local_repository` in Bazel repository definitions (e.g., `repos.bzl`) is strictly forbidden across all submodules."
  - **Impact:** Doing so breaks hermeticity and cross-system reproducibility.
  - **Action:** Never use `local_repository` in Bazel definitions; strictly enforce hermetic dependencies.

- **Bazel Header Resolution Constraints**
  - **Quote:** "When including headers from `mpact-sim`... the corresponding Bazel `cc_library` target in `mpact-riscv` MUST explicitly include the exact dependency target name... in its `deps` list."
  - **Impact:** Failing to declare explicit dependencies results in fatal "file not found" compilation errors.
  - **Action:** Always explicitly include exact dependency target names in `deps` lists for Bazel `cc_library` targets.

- **Struct Definition Duplication Across Submodules**
  - **Quote:** "When multiple repositories define the exact same struct (e.g., `rvvi_trace_event_t`) for ABI plugin compliance, they must wrap the definition in `#ifndef` guards..."
  - **Impact:** Cross-repository compilation and linking will fail with fatal C++ `typedef redefinition` errors.
  - **Action:** Always wrap duplicate struct definitions in `#ifndef` guards (e.g., `#ifndef RVVI_TRACE_EVENT_T_DEFINED`).

### Tier 2: Memory Safety, Code Quality & Standard Practices

- **absl::StatusOr Pointer Unwrapping Ban**
  - **Quote:** "Never unwrap `absl::StatusOr<T>` using the `*` pointer operator (e.g., `*res`)."
  - **Impact:** Violates strict memory safety boundaries and risks pointer dereference undefined behavior.
  - **Action:** Always explicitly call `.value()` after checking `.ok()` when unwrapping `absl::StatusOr<T>`.

- **RISC-V RMM Rounding Mode Fidelity**
  - **Quote:** "When implementing operations that respect the `RMM` (Round to Nearest, ties to Max Magnitude) rounding mode, remember that `ScopedFPStatus` translates it to standard ties-to-even on most hosts."
  - **Impact:** Relying on `ScopedFPStatus` defaults to standard ties-to-even, violating architectural fidelity for RMM.
  - **Action:** Manually intercept `kRoundToNearestTiesToMax` and apply `std::round` (which intrinsically rounds halves away from zero) to ensure correct architectural fidelity.

- **Floating-Point Boundaries**
  - **Quote:** "Avoid hardcoding raw float maximums directly in inline code logic. Avoid using `std::pow` as it is not `constexpr` and introduces hot-path overhead."
  - **Impact:** Hardcoding introduces magic numbers and `std::pow` introduces hot-path execution overhead.
  - **Action:** Assign float maximums explicitly to named `constexpr`/`const` constants (e.g., `constexpr double kTwoPow32 = 4294967296.0;`).

- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**
  - **Quote:** "Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`)."
  - **Impact:** Violates DRY principles and complicates maintenance across architectural variants.
  - **Action:** Extract duplicate trace formatting logic into reusable utilities like `TraceFormatter`.

- **CsrDirtyList Implementation Status**
  - **Quote:** "Successfully integrated `CsrDirtyList` into `mpact-riscv/riscv/riscv_csr.h` to track modified CSR addresses."
  - **Impact:** Ensures tracking of modified CSR addresses is correctly maintained.
  - **Action:** Continue utilizing `CsrDirtyList` for CSR modification tracking across architectural components.