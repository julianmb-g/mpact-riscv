# mpact-riscv Agent Instructions

## Lessons Learned & Orchestration Rules

### Tier 1: Critical Blocker

* **RVVI ABI Fidelity Enforcement**
  * **Quote:** "The lock-free rvvi_trace_event_t POD struct must be exactly 64 bytes padded. However, an inspection reveals the struct is tracking 128 bytes."
  * **Impact:** Breaking the strict 64-byte ABI alignment for tracing structures silently corrupts cross-component ring buffer integrations and breaks trace event ingestion, even if tests appear to pass locally.
  * **Action:** Never alter foundational ABI size assertions. Any size assertions MUST exactly match the architectural `DESIGN.md`.

* **Zicbom Standard Profile Execution Mocking**
  * **Quote:** "The test directly injects the semantic function into a mocked instruction object and manually executes it, completely bypassing the Instruction Decoder. In `riscv_zicbo_instructions_test.cc`, `instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboZero); instruction_->Execute(nullptr);`"
  * **Impact:** Total evasion of the E2E execution boundary. Mocks the instruction object bypassing the decoder and memory interface.
  * **Action:** Write a strict E2E test that cross-compiles Zicbom assembly (`cbo.zero`, `cbo.clean`), loads the ELF into the `RiscvTop` simulator naturally, and verifies the architectural state/trap handling natively through the CPU loop.

* **Isolated Execution Boundary Evaluation**
  * **Quote:** "The test evaluates the C++ loader function natively on `FlatDemandMemory` but never executes the loaded payload via the simulator's CPU loop."
  * **Impact:** Asserting memory is written completely ignores whether the `RiscvTop` orchestrator can accurately execute the loaded bytes. This represents an isolated unit test masking as E2E.
  * **Action:** Any payload loading test MUST instantiate the `RiscvTop` execution loop and organically step the CPU, asserting `pc` bounds traverse the mapped addresses natively.

* **Destructive RMW on MMIO Peripherals**
  * **Quote:** "Unaligned or partial-word RVVI writes MUST be executed as an aligned 64-bit Read-Modify-Write (RMW) cycle."
  * **Impact:** Executing a 64-bit RMW on an MMIO boundary reads volatile hardware registers and writes back, corrupting peripheral state.
  * **Action:** Agents MUST exempt MMIO address spaces from the 64-bit RMW mandate and ensure explicit byte-lane masking support for device I/O regions.

* **SPSC Ring Buffer Concurrency & Deadlocks**
  * **Quote:** "SPSC buffer trace synchronization, state export, and backpressure blocking."
  * **Impact:** Fails to natively enforce backpressure, causing infinite timeout loops, CPU starvation, or masking deadlocks.
  * **Action:** Initialize `AsyncFormattingDaemon` and `RvviMemoryMapper` before `riscv_top.Run()`. Implement lock-free SPSC boundaries where producers yield. Evaluate against `kSpscYieldTimeoutMs = 5000` and throw `std::runtime_error` to natively evaluate via `EXPECT_THROW`.

* **Struct Alignment & ABI Compliance**
  * **Quote:** "rvvi_trace_event_t struct sizing, duplication across submodules, and memory population."
  * **Impact:** Cache-line bounds violations, ABI corruption, and fatal `typedef redefinition` during linking.
  * **Action:** Constrain `rvvi_trace_event_t` to strict 64-byte alignment with `#ifndef` guards. Prune contradictory legacy definitions. Use native assignments or `std::memcpy` instead of `absl::StrAppendFormat`.

* **Simulator Memory Mappers Lifecycle**
  * **Quote:** "Instantiating RvviMemoryMapper pointers without structural lifecycle checks."
  * **Impact:** Memory leaks or fatal segmentation faults due to unmapped interfaces.
  * **Action:** Guard mappers with `std::unique_ptr`. Update the base `MemoryInterface` pointer before initializing subsequent structures.

* **Cosmetic RVVI Trivialization Ban**
  * **Quote:** "Immediately sending 'quit' to interactive CLI wrappers to bypass trace generation."
  * **Impact:** Masks genuine execution and trace logging testing.
  * **Action:** Tests must organically route payloads to natively execute and format RVVI traces.

* **Authentic Execution Boundaries & E2E Verification**
  * **Quote:** "Validating bytes written to memory does not prove cross-component hardware integration. Eradicate `memory->Store` mock logic with hardcoded opcodes."
  * **Impact:** Mocking instruction boundaries, using isolated target encoders, or checking empty `#include` bytes creates systemic testing illusions, masking E2E routing and architectural trapping failures.
  * **Action:** E2E execution tests MUST natively route authentic cross-compiled ELFs through the full simulator loop (`RiscVTop::Step()`). Dynamically compile payloads using `NativeTextualAssembler` to verify organic execution. Never mock E2E boot tests or inject raw hexadecimal words.

* **Hardware Interrupt Testing Authenticity**
  * **Quote:** "When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually."
  * **Impact:** Manual evaluation creates mock boundaries that fail to prove actual execution trapping.
  * **Action:** Instantiate the full `RiscVTop` CPU execution loop, use `NativeTextualAssembler` to compile authentic RISC-V payloads (like `wfi`), and verify organic traps and correct `epc` updates.

* **FDT Magic Size Boundary Evasion**
  * **Quote:** "`mpact-riscv/riscv/riscv_dtb_loader.cc` Mutated `if (dtb_size >= 4)` to `if (dtb_size > 4)`"
  * **Impact:** Skips device tree magic validation block if exactly 4-byte FDT magic header is passed.
  * **Action:** Explicitly inject an exactly 4-byte DTB payload (containing only magic `0xd00dfeed`) to enforce exact boundary natively.

* **Test Pyramid & Execution Verification (MANDATE 3)**
  * **Quote:** "Any new feature or pipeline (like Zfa rounding in mpact-riscv) MUST enforce a strict Test Pyramid: an executing ELF payload evaluated by the top-level simulator."
  * **Impact:** Isolated string matching or individual instruction evaluations without cross-component hardware integration are flagged as systemic testing illusions.
  * **Action:** Ensure new features strictly test via an executing ELF payload at the top-level simulator.

* **Authentic RVA23 Validation Artifact Trapping**
  * **Quote:** "Using `GTEST_SKIP()` when the `vmlinux_placeholder.elf` artifact is missing."
  * **Impact:** Muting the test prevents organic discovery of missing build artifacts or compilation failures, silently eviscerating RVA23 boot validation boundaries.
  * **Action:** Agents MUST forbid `GTEST_SKIP()` in Boot Sequence validation tests and replace it with hard failure assertions (`ASSERT_TRUE(load_status.ok())`) to ensure organic pipeline trapping.

### Tier 2: System Architecture

* **Bazel Hermeticity & Ledger Preservation**
  * **Quote:** "Replacing http_archive with local_repository or applying sweeping text replacements to ledgers."
  * **Impact:** Corrupts unrelated components, breaks hermeticity, and destroys cross-system reproducibility.
  * **Action:** Enforce block-based regex pruning for ledgers. Prohibit `local_repository` usage. Reference exact upstream targets (e.g., `@com_google_mpact-riscv//riscv:rv32g_sim`).

* **Dynamic Plugin Architecture for RVVI**
  * **Quote:** "Exposing RVVI trace events as a dynamic plugin (libmpact_rvvi.so)."
  * **Impact:** External simulation environments fail to `dlopen()` the ABI-compliant interface.
  * **Action:** Build the plugin as a `cc_binary` target with `linkshared = True`, depending directly on the `rvvi_abi` target.

* **Zve32f Architecture Extraction**
  * **Quote:** "Redefining floating point instructions from scratch for CoralNPU M3."
  * **Impact:** Violates architectural requirements by duplicating code instead of leveraging upstream.
  * **Action:** Extract the Zve32f instruction set strictly from the reference mpact-riscv `.isa` and `.bin_fmt` files and migrate them.

* **Opaque OS Boot Register Handoff Illusion**
  * **Quote:** "The E2E OS boot test must execute an authentic OS payload that organically reads `a0` and `a1` and writes their values out to a verifiable memory address."
  * **Impact:** Masks boot payload execution failures and allows tests to blindly execute `NOP` space, evading genuine step-through verification.
  * **Action:** Ensure the boot test executes an authentic OS payload reading `a0`/`a1` and writing values to verified memory.