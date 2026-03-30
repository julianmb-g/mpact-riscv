# mpact-riscv Agent Instructions

## Lessons Learned & Orchestration Rules

### Tier 1: Critical Blocker

* **Authentic Execution Boundaries & E2E Verification**
  * **Quote:** "Validating bytes written to memory does not prove cross-component hardware integration. Eradicate `memory->Store` mock logic with hardcoded opcodes."
  * **Impact:** Mocking instruction boundaries, using isolated target encoders, or checking empty `#include` bytes creates systemic testing illusions, masking E2E routing and architectural trapping failures.
  * **Action:** E2E execution tests MUST natively route authentic cross-compiled ELFs through the full simulator loop (`RiscVTop::Step()`). Dynamically compile payloads using `NativeTextualAssembler` to verify organic execution. Never mock E2E boot tests or inject raw hexadecimal words.

* **Authentic RVA23 Validation Artifact Trapping**
  * **Quote:** "Using `GTEST_SKIP()` when the `vmlinux_placeholder.elf` artifact is missing."
  * **Impact:** Muting the test prevents organic discovery of missing build artifacts or compilation failures, silently eviscerating RVA23 boot validation boundaries.
  * **Action:** Agents MUST forbid `GTEST_SKIP()` in Boot Sequence validation tests and replace it with hard failure assertions (`ASSERT_TRUE(load_status.ok())`) to ensure organic pipeline trapping.

* **Cosmetic RVVI Trivialization Ban**
  * **Quote:** "Immediately sending 'quit' to interactive CLI wrappers to bypass trace generation."
  * **Impact:** Masks genuine execution and trace logging testing.
  * **Action:** Tests must organically route payloads to natively execute and format RVVI traces.

* **Destructive RMW on MMIO Peripherals**
  * **Quote:** "Unaligned or partial-word RVVI writes MUST be executed as an aligned 64-bit Read-Modify-Write (RMW) cycle."
  * **Impact:** Executing a 64-bit RMW on an MMIO boundary reads volatile hardware registers and writes back, corrupting peripheral state.
  * **Action:** Agents MUST exempt MMIO address spaces from the 64-bit RMW mandate and ensure explicit byte-lane masking support for device I/O regions.

* **FDT Magic Size Boundary Evasion**
  * **Quote:** "`mpact-riscv/riscv/riscv_dtb_loader.cc` Mutated `if (dtb_size >= 4)` to `if (dtb_size > 4)`"
  * **Impact:** Skips device tree magic validation block if exactly 4-byte FDT magic header is passed.
  * **Action:** Explicitly inject an exactly 4-byte DTB payload (containing only magic `0xd00dfeed`) to enforce exact boundary natively.

* **Hardware Interrupt Testing Authenticity**
  * **Quote:** "When implementing hardware interrupt tests (like CLINT/PLIC logic), tests must not just evaluate `is_interrupt_available` manually."
  * **Impact:** Manual evaluation creates mock boundaries that fail to prove actual execution trapping.
  * **Action:** Instantiate the full `RiscVTop` CPU execution loop, use `NativeTextualAssembler` to compile authentic RISC-V payloads (like `wfi`), and verify organic traps and correct `epc` updates.

* **Isolated Execution Boundary Evaluation**
  * **Quote:** "The test evaluates the C++ loader function natively on `FlatDemandMemory` but never executes the loaded payload via the simulator's CPU loop."
  * **Impact:** Asserting memory is written completely ignores whether the `RiscvTop` orchestrator can accurately execute the loaded bytes. This represents an isolated unit test masking as E2E.
  * **Action:** Any payload loading test MUST instantiate the `RiscvTop` execution loop and organically step the CPU, asserting `pc` bounds traverse the mapped addresses natively.

* **RVVI ABI Fidelity Enforcement**
  * **Quote:** "The lock-free rvvi_trace_event_t POD struct must be exactly 64 bytes padded. However, an inspection reveals the struct is tracking 128 bytes."
  * **Impact:** Breaking the strict 64-byte ABI alignment for tracing structures silently corrupts cross-component ring buffer integrations and breaks trace event ingestion, even if tests appear to pass locally.
  * **Action:** Never alter foundational ABI size assertions. Any size assertions MUST exactly match the architectural `DESIGN.md`.

* **Simulator Memory Mappers Lifecycle**
  * **Quote:** "Instantiating RvviMemoryMapper pointers without structural lifecycle checks."
  * **Impact:** Memory leaks or fatal segmentation faults due to unmapped interfaces.
  * **Action:** Guard mappers with `std::unique_ptr`. Update the base `MemoryInterface` pointer before initializing subsequent structures.

* **SPSC Ring Buffer Concurrency & Deadlocks**
  * **Quote:** "SPSC buffer trace synchronization, state export, and backpressure blocking."
  * **Impact:** Fails to natively enforce backpressure, causing infinite timeout loops, CPU starvation, or masking deadlocks.
  * **Action:** Initialize `AsyncFormattingDaemon` and `RvviMemoryMapper` before `riscv_top.Run()`. Implement lock-free SPSC boundaries where producers yield. Evaluate against `kSpscYieldTimeoutMs = 5000` and throw `std::runtime_error` to natively evaluate via `EXPECT_THROW`.

* **Struct Alignment & ABI Compliance**
  * **Quote:** "rvvi_trace_event_t struct sizing, duplication across submodules, and memory population."
  * **Impact:** Cache-line bounds violations, ABI corruption, and fatal `typedef redefinition` during linking.
  * **Action:** Constrain `rvvi_trace_event_t` to strict 64-byte alignment with `#ifndef` guards. Prune contradictory legacy definitions. Use native assignments or `std::memcpy` instead of `absl::StrAppendFormat`.

* **Test Pyramid & Execution Verification (MANDATE 3)**
  * **Quote:** "Any new feature or pipeline (like Zfa rounding in mpact-riscv) MUST enforce a strict Test Pyramid: an executing ELF payload evaluated by the top-level simulator."
  * **Impact:** Isolated string matching or individual instruction evaluations without cross-component hardware integration are flagged as systemic testing illusions.
  * **Action:** Ensure new features strictly test via an executing ELF payload at the top-level simulator.

### Tier 2: System Architecture

* **Bazel Hermeticity & Ledger Preservation**
  * **Quote:** "Replacing http_archive with local_repository or applying sweeping text replacements to ledgers."
  * **Impact:** Corrupts unrelated components, breaks hermeticity, and destroys cross-system reproducibility.
  * **Action:** Enforce block-based regex pruning for ledgers. Prohibit `local_repository` usage. Reference exact upstream targets (e.g., `@com_google_mpact-riscv//riscv:rv32g_sim`).

* **Dynamic Plugin Architecture for RVVI**
  * **Quote:** "Exposing RVVI trace events as a dynamic plugin (libmpact_rvvi.so)."
  * **Impact:** External simulation environments fail to `dlopen()` the ABI-compliant interface.
  * **Action:** Build the plugin as a `cc_binary` target with `linkshared = True`, depending directly on the `rvvi_abi` target.

* **Opaque OS Boot Register Handoff Illusion**
  * **Quote:** "The E2E OS boot test must execute an authentic OS payload that organically reads `a0` and `a1` and writes their values out to a verifiable memory address."
  * **Impact:** Masks boot payload execution failures and allows tests to blindly execute `NOP` space, evading genuine step-through verification.
  * **Action:** Ensure the boot test executes an authentic OS payload reading `a0`/`a1` and writing values to verified memory.

* **Zve32f Architecture Extraction**
  * **Quote:** "Redefining floating point instructions from scratch for CoralNPU M3."
  * **Impact:** Violates architectural requirements by duplicating code instead of leveraging upstream.
  * **Action:** Extract the Zve32f instruction set strictly from the reference mpact-riscv `.isa` and `.bin_fmt` files and migrate them.

### Architectural Design & API Contracts

* **CLINT/PLIC Hardware Interrupt State Management**
  * **Quote:** "Test CLINT Machine Software Interrupt without setting MIE flags."
  * **Impact:** Missing `state_->mie()->set_msie(1)` causes software interrupts (Exception Code 3) to be silently dropped despite MSIP assertion, breaking test boundaries.
  * **Action:** When validating hardware interrupts like CLINT MSIP, ensure the architectural Machine Interrupt Enable (MIE) bit for the specific source (`MSIE`, `MTIE`, `MEIE`) is explicitly activated in `RiscVState` prior to stepping the simulator.

* **FDT Magic Number Strictness & Missing Artifact Mandate**
  * **Quote:** "Ensure the test asserts the OpenSBI boot handshake (0xd00dfeed) and strictly uses FAIL() if the authentic pre-compiled vmlinux payload is missing."
  * **Impact:** Using `GTEST_SKIP()` or dummy generators when essential artifacts are absent masks the systemic failure to cross-compile payloads, allowing testing fraud to bypass CI execution constraints.
  * **Action:** Boot tests must strictly utilize `FAIL()` on missing artifacts and physically assert the `0xd00dfeed` OpenSBI handoff magic number is loaded into memory.

*   **Mandate:** Ensure all unbuilt requirements and architectural designs reflect clear HW/SW boundaries, exact file paths, and strict API/ABI contracts. Use Mermaid for topology when defining mpact-riscv.

* **Static Pointer Caching & Dangling References**
  * **Quote:** "Using `static mpact::sim::riscv::RiscVCsrInterface* cached_menvcfg` to cache a CSR pointer across multiple tests."
  * **Impact:** State components like `RiscVState` are recreated per test. Static locals persist across the test suite, leading to dangling pointers, memory corruption, and segmentation faults when subsequent tests access the destroyed memory.
  * **Action:** Never use `static` local variables to cache pointers to objects with instance-level lifecycles. Always cache these pointers as member variables (e.g., `cached_menvcfg_` in the class definition) and initialize them in the constructor.

* **Svadu (Hardware A/D bit updates) and Mock PTE Dependencies**
  * **Quote:** "Adding new A and D bit fault checks completely broke old MMU tests because they used mock PTEs without 0xC0."
  * **Impact:** Tightening architectural constraints (like checking A/D bits natively) organically exposes fragile mock test environments.
  * **Action:** When introducing strict PTE permission constraints (Svadu, Svpbmt), ALL legacy MMU tests must be proactively patched to include required mock permission bits (e.g., `| 0xC0`) rather than disabling the constraint check itself.

* **The Mocked Boundary Illusion: mpact-riscv ZFA Execution Void**
  * **Quote:** "Complex Zfa semantics tests in riscv_zfa_instructions_test.cc claim comprehensive test vectors but exclusively evaluate raw generic::Instruction objects with explicitly mocked operands."
  * **Impact:** Zero integration coverage ensuring an authentic, cross-compiled ELF utilizing Zfa arithmetic routes securely through the rv64g_sim instruction loop.
  * **Action:** Zfa execution tests MUST cross-compile authentic Zfa assembly, load the ELF into the RiscVTop simulator natively, and verify the architectural state/trap handling natively through the CPU loop rather than directly invoking generic::Instruction mocks.

### Orchestration Execution Insights (Cycle 165 - Build Agent)

* **RVVI Plugin Architecture Target Availability**: Building `libmpact_rvvi.so` requires explicitly declaring a `cc_binary` target with `linkshared = True` in `mpact-riscv/riscv/BUILD`, linking against the existing trace interfaces to satisfy external simulation environment bounds.

### Globally Relevant Execution Rules (Appended)

### Orchestration Execution Insights (Cycle 166)

* **Oversized Vector Trace Atomicity**: A single 64-byte `TraceEvent` cannot contain the full state delta for massive vector loads (e.g., `vl8re8.v`). Multi-register architectural updates must be atomized using `fragment_index`/`is_last` boolean flags.

* **Pre-compiled OS Boot Fallback Mechanism**: Purge placeholder payload logic. Formally enforce that all Linux boot tests MUST organically `FAIL()` if the authentic `.elf` payload is missing.

* **RVVI SPSC Formatting Daemon Crash Deadlock**: The ring buffer must include a cross-thread health check (`std::atomic<bool> daemon_alive`) to cleanly abort the main simulator loop if the consumer thread terminates.

### Orchestration Execution Insights (Cycle 166 - Boot Validation)

* **Boot Protocol Memory Validation Strictness**:
  * **Quote:** "Boot tests must execute an authentic OS payload that organically reads a0 (hartid) and a1 (.dtb) and asserts non-intersection bounds checks."
  * **Impact:** Validating `EXPECT_TRUE(status.ok())` from `LinuxKernelBootloader::Load` without physically asserting memory at `0x21000000` is a cosmetic test. It fails to guarantee the FDT magic number `0xd00dfeed` is accessible to the NPU execution.
  * **Action:** Boot handshake execution tests MUST physically inject `0xd00dfeed` into the mapped DTB memory and `EXPECT_EQ(mem_db->Get<uint32_t>(0), 0xd00dfeed)` before starting the execution loop, asserting it does not intersect with the `vmlinux` payload.

# mpact-riscv Submodule Execution Directives

## Trace Fidelity

* **RVVI Oracle Fidelity ("Sum of Deltas" Theorem)**: When validating hardware tracing APIs, it is strictly forbidden to use cosmetic evaluation limits like `EXPECT_EQ(output, "PASS")`. Implement explicit temporal limits and mathematically accumulate structural deltas to natively re-derive and verify the final hardware state.

* **Zfa Execution Void**: Complex Zfa semantics tests MUST NOT exclusively evaluate raw `generic::Instruction` objects with explicitly mocked operands. You MUST ensure an authentic, cross-compiled ELF utilizing Zfa arithmetic routes securely through the `rv64g_sim` instruction loop.

### Orchestration Execution Insights (Cycle 166 - Smcntrpmf)

* **Testing Fraud vs. Authentic Performance Counters**: When testing `Smcntrpmf` counting inhibitions (`mcyclecfg`, `minstretcfg`), never test by statically incrementing variables or injecting manual values into registers. You MUST instantiate `RiscVTop` using `FlatDemandMemory` and execute raw sequential RISC-V instructions (`0x00000013` NOPs) natively through `Step()` to mathematically prove the architectural counter pauses organically in the specified privilege mode.

### Orchestration Execution Insights (Cycle 166 - Smcntrpmf)
