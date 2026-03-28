# mpact-riscv Agent Instructions

## Lessons Learned

### Tier 1: Critical Architecture, Testing Boundaries & Build Integrity

- **Authentic Execution Boundaries & E2E Verification**
  - **Quote:** "Validating bytes written to memory does not prove cross-component hardware integration. Eradicate `memory->Store` mock logic with hardcoded opcodes."
  - **Impact:** Mocking instruction boundaries, using isolated target encoders, or checking empty `#include` bytes creates systemic testing illusions, masking E2E routing and architectural trapping failures.
  - **Action:** E2E execution tests MUST natively route authentic cross-compiled ELFs through the full simulator loop (`RiscVTop::Step()`). Dynamically compile payloads using `NativeTextualAssembler` to verify organic execution. Never mock E2E boot tests or inject raw hexadecimal words.

- **Hardware Simulation & Memory Isolation Rules**
  - **Quote:** "Mocking AxiSlave with Python dictionaries and swallowing test exceptions."
  - **Impact:** False positive 100% unit tests that fail to simulate RTL component boundaries.
  - **Action:** Tests mimicking external memory MUST instantiate REAL synthesized DDR controllers and SRAM RTL block responders. Diagnose AXI Memory Interface drops directly. Eviscerating memory boundaries to trap timeouts is strictly forbidden.

- **OS Boot Sequence & Memory Offset Bounds**
  - **Quote:** "Validating that bytes are written to memory does not prove the simulator can actually execute the OpenSBI boot handshake."
  - **Impact:** Fails to prevent overlapping memory regions and invalid Device Tree Blobs (DTB) from silently crashing the bootloader.
  - **Action:** Enforce strict physical load addresses (e.g., `vmlinux` at `0x20000000`, DTB at `0x21000000`) and assert non-intersection. Boot tests must instantiate the top-level simulator and execute the trace natively. FDT magic numbers must be strictly checked and OS payloads physically routed. If OS artifacts are completely missing, enforce organic failure (e.g., `absl::IsNotFound` or `unittest.SkipTest`).

- **CLI Argument Boundary Verification**
  - **Quote:** "Execution binaries (e.g., `coralnpu_v2_sim.cc`) must be organically tested with `argc == 0, 1, and 3+`."
  - **Impact:** Prevents "Happy-Path" verification bias that obscures execution bounds defects.
  - **Action:** Dynamically invoke execution binaries with missing and excessive arguments to strictly assert valid trapping of unhandled limits.

- **Hardware Interrupt Testing Authenticity**
  - **Quote:** "When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually."
  - **Impact:** Manual evaluation creates mock boundaries that fail to prove actual execution trapping.
  - **Action:** Instantiate the full `RiscVTop` CPU execution loop, use `NativeTextualAssembler` to compile authentic RISC-V payloads (like `wfi`), and verify organic traps and correct `epc` updates.

- **RVVI ABI Fidelity Enforcement**
  - **Quote:** "The lock-free rvvi_trace_event_t POD struct must be exactly 64 bytes padded."
  - **Impact:** Breaking the strict 64-byte ABI alignment for tracing structures silently corrupts cross-component ring buffer integrations and breaks trace event ingestion.
  - **Action:** Never alter foundational ABI size assertions. Any size assertions MUST exactly match the architectural `DESIGN.md`.

- **Mathematical Trace Fidelity**
  - **Quote:** "When validating hardware tracing APIs, it is strictly forbidden to use cosmetic evaluation limits like `EXPECT_EQ(output, "PASS")`."
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

- **Zicbom Standard Profile Execution Mocking**
  - **Quote:** "In `riscv_zicbo_instructions_test.cc`, `instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboZero); instruction_->Execute(nullptr);`"
  - **Impact:** Total evasion of the E2E execution boundary. Mocks the instruction object bypassing decoder and memory interface.
  - **Action:** MUST write strict E2E test that cross-compiles Zicbom assembly (`cbo.zero`), loads ELF into `RiscvTop` simulator naturally, and verifies state natively.

- **FDT Magic Size Boundary Evasion**
  - **Quote:** "`mpact-riscv/riscv/riscv_dtb_loader.cc` Mutated `if (dtb_size >= 4)` to `if (dtb_size > 4)`"
  - **Impact:** Skips device tree magic validation block if exactly 4-byte FDT magic header is passed.
  - **Action:** MUST explicitly inject an exactly 4-byte DTB payload (containing only magic `0xd00dfeed`) to enforce exact boundary natively.

- **OS Boot Entry Point Boundary Evasion**
  - **Quote:** "`mpact-riscv/riscv/riscv_dtb_loader.cc` Mutated `if (0x20000000 >= start)`..."
  - **Impact:** Falsely rejects valid ELF payloads that begin exactly at `0x20000000`.
  - **Action:** Generate rigorous ELF payload that aligns exactly to `0x20000000` to verify boundary conditions are inclusive natively.

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

- **Python Linter Integrity**
  - **Quote:** "Scattered Python import violations must be resolved by moving standard/third-party imports to the top of the file."
  - **Impact:** Violates style guidelines and fragments module layout.
  - **Action:** Move imports to the top of the file (exceptions apply to PyBind11 bindings). Use verifiable assertions for unused variables.

- **Code Duplication & Component Reusability**
  - **Quote:** "Both top-level simulators duplicate logic for `DataBuffer` trace formatting."
  - **Impact:** Violates DRY principles and complicates maintenance across architectural variants.
  - **Action:** Extract duplicate logic (like trace formatting or `CsrDirtyList` tracking) into reusable utilities and common headers.

- **Submodule Ledger Consolidation**
  - **Quote:** "Leaving 'Restored Knowledge' blocks at the bottom of the submodule AGENTS.md."
  - **Impact:** Fragments submodule-specific execution constraints.
  - **Action:** Immediately integrate audit restorations into the primary strict execution mandates and remove the restoration headers.

* **OS Boot Entry Point Boundary Evasion**
  * **Quote:** "Mutating if (0x20000000 >= start && 0x20000000 < end) to if (0x20000000 > start && 0x20000000 < end)"
  * **Impact:** This surviving mutant falsely rejects valid ELF payloads that begin exactly at the `0x20000000` entry point address, proving the E2E hardware verification is failing to organically test perfect edge-case boundary mapping for the OS boot payload.
  * **Action:** `riscv_dtb_loader_test` MUST generate a rigorous ELF payload that perfectly aligns exactly to `0x20000000` to verify boundary conditions are inclusive natively.
