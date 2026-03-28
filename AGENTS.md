# mpact-riscv Agent Instructions

## Lessons Learned

### Tier 1: Critical Architecture, Testing Boundaries & Build Integrity

- **Authentic Execution Boundaries & E2E Verification**
  - **Quote:** "Validating bytes written to memory does not prove cross-component hardware integration. Eradicate `memory->Store` mock logic with hardcoded opcodes."
  - **Impact:** Mocking instruction boundaries or using isolated target encoders creates systemic testing illusions, masking E2E routing and architectural trapping failures.
  - **Action:** E2E execution tests MUST natively route authentic cross-compiled ELFs through the full simulator loop (`RiscVTop::Step()`). Dynamically compile payloads using `NativeTextualAssembler` to verify organic execution.

- **Hardware Simulation & Mock Isolation Rules**
  - **Quote:** "Mocking AxiSlave with Python dictionaries and swallowing test exceptions."
  - **Impact:** False positive 100% unit tests that fail to simulate RTL component boundaries.
  - **Action:** Tests mimicking external memory MUST instantiate REAL synthesized DDR controllers and SRAM RTL block responders. Eviscerating memory boundaries to trap timeouts is strictly forbidden.

- **Hardware Interrupt Testing Authenticity**
  - **Quote:** "When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually."
  - **Impact:** Manual evaluation creates mock boundaries that fail to prove actual execution trapping.
  - **Action:** Instantiate the full `RiscVTop` CPU execution loop, use `NativeTextualAssembler` to compile authentic RISC-V payloads (like `wfi`), and verify organic traps and correct `epc` updates.

- **Boot Sequence Integrity & Memory Offset Bounds**
  - **Quote:** "Validating the OpenSBI hardware handshake tests must utilize mock/dummy payloads and enforce organic failure via `absl::IsNotFound` when artifacts are completely missing."
  - **Impact:** Using `GTEST_SKIP()` mathematically masks configuration drift. Overlapping memory regions cause silent bootloader crashes.
  - **Action:** Enforce strict physical load addresses (e.g., `vmlinux` at `0x20000000`, DTB at `0x21000000`) and assert non-intersection. Enforce organic bounds failures natively when artifacts are missing.

- **Mathematical Trace Fidelity**
  - **Quote:** "When validating hardware tracing APIs, it is strictly forbidden to use cosmetic evaluation limits like `EXPECT_EQ(output, \"PASS\")`."
  - **Impact:** Cosmetic assertions fail to guarantee structural state correctness of the hardware.
  - **Action:** Implement explicit temporal limits and mathematically accumulate structural deltas to natively re-derive the hardware state.

- **Hermeticity & Dependency Resolution**
  - **Quote:** "Replacing `http_archive` with `local_repository` is strictly forbidden. The Bazel `cc_library` target MUST explicitly include the exact dependency target name."
  - **Impact:** Breaks hermeticity, cross-system reproducibility, and causes fatal "file not found" compilation errors.
  - **Action:** Never use `local_repository`. Always explicitly list exact target names in `deps` for Bazel targets.

- **Struct Definition Duplication**
  - **Quote:** "When multiple repositories define the exact same struct for ABI plugin compliance, they must wrap the definition in `#ifndef` guards."
  - **Impact:** Cross-repository linking fails with fatal C++ `typedef redefinition` errors.
  - **Action:** Always wrap duplicate struct definitions in `#ifndef` guards (e.g., `#ifndef RVVI_TRACE_EVENT_T_DEFINED`).

### Tier 2: Memory Safety, Code Quality & Standard Practices

- **absl::StatusOr Pointer Unwrapping Ban**
  - **Quote:** "Never unwrap `absl::StatusOr<T>` using the `*` pointer operator."
  - **Impact:** Violates strict memory safety boundaries and risks pointer dereference undefined behavior.
  - **Action:** Always explicitly call `.value()` after checking `.ok()`.

- **RISC-V RMM Rounding Mode Fidelity**
  - **Quote:** "ScopedFPStatus translates RMM (Round to Nearest, ties to Max Magnitude) to standard ties-to-even on most hosts."
  - **Impact:** Relying on default host rounding violates architectural fidelity for RMM instructions.
  - **Action:** Manually intercept `kRoundToNearestTiesToMax` and apply `std::round` to ensure correct architectural behavior.

- **Floating-Point Boundaries & Performance**
  - **Quote:** "Avoid hardcoding raw float maximums directly in inline code logic. Avoid using `std::pow` as it is not `constexpr`."
  - **Impact:** Introduces magic numbers and hot-path execution overhead.
  - **Action:** Assign float maximums explicitly to named `constexpr`/`const` constants (e.g., `constexpr double kTwoPow32 = 4294967296.0;`).

- **Code Duplication & Component Reusability**
  - **Quote:** "Both top-level simulators duplicate logic for `DataBuffer` trace formatting."
  - **Impact:** Violates DRY principles and complicates maintenance across architectural variants.
  - **Action:** Extract duplicate logic (like trace formatting or `CsrDirtyList` tracking) into reusable utilities and common headers.

- **Submodule Ledger Consolidation**
  - **Quote:** "Leaving 'Restored Knowledge' blocks at the bottom of the submodule AGENTS.md."
  - **Impact:** Fragments submodule-specific execution constraints.
  - **Action:** Immediately integrate audit restorations into the primary strict execution mandates and remove the restoration headers.

### New Lessons Learned (Cycle 166)
* **Deadlock Masking via Timeouts:** Trapping `SimTimeoutError` with artificial bounds (`with_timeout()`) masks RTL stalling loops without fixing them. Diagnose AXI Memory Interface drops directly and migrate from `AxiSlave` Python dictionaries to synthesized DDR controllers.
* **CLI Bounds and Argument Parsing:** Execution binaries (e.g., `coralnpu_v2_sim.cc`) must be organically tested with `argc == 0, 1, and 3+` to ensure they natively catch unhandled limits, preventing "Happy-Path" verification bias.
* **E2E Integration Testing Rigor:** Mocking components like `TargetEncoder` or injecting raw hexadecimal words into memory (e.g., bypassing `NativeTextualAssembler`) is testing fraud. Authentic tests must route raw assembly through the full compilation-to-execution loop.
* **Python Linter Integrity:** Scattered Python import violations must be resolved by moving standard/third-party imports to the top of the file (exceptions apply to PyBind11 simulator bindings). Remove unused variables or use them in verifiable assertions.
* **OS Boot Artifact Graceful Degradation:** Pre-compiled OS artifacts must be probed; if missing, raise `unittest.SkipTest` or `pytest.skip` organically to avoid pipeline-crashing null pointer defects.

### New Lessons Learned (Cycle 168 - E2E Fidelity)
* **Tier 1: Phantom E2E Boot Test Reversion:** 
  * **Quote:** "Validating OS bootstraps by only checking bytes written to memory or executing arbitrary NOPs."
  * **Impact:** Empty `#include` validation fraud creates a false sense of OS boot execution.
  * **Action:** Never mock E2E boot tests. FDT magic numbers must be strictly checked and OS payloads physically routed. Revert empty validation tests.
