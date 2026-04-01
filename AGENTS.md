# mpact-riscv Agent Instructions

### Core Execution Integrity
* **Cosmetic RVVI Trivialization Ban**: Tests must natively execute and format RVVI traces. Do not immediately send "quit" to interactive CLI wrappers to bypass trace generation logic.
* **Isolated Execution Evaluation**: Asserting memory writes without checking the CPU orchestrator executes the bytes is testing fraud. Payload loading tests MUST instantiate the `RiscvTop` execution loop and step the CPU natively.
* **Test Pyramid & Execution Verification (MANDATE 3)**: New features MUST enforce a strict Test Pyramid: an executing ELF payload evaluated natively by the top-level simulator. Isolated string matching or individual instruction evals without hardware integration are flagged.
* **Testing Illusion Review**: Verify all bounds are strictly checked. No bypassing.
* **Fake Integration & Raw Bytes Injection**: You MUST NOT use `NativeTextualAssembler` to directly inject raw bytecode strings via `.word` into `FlatDemandMemory`. This completely mocks the architectural execution pipeline boundary and `ElfProgramLoader` parsing logic. You must manually cross-compile real `.elf` payloads utilizing the local `riscv64-unknown-elf-gcc -nostdlib` compiler, check in both `.s` and `.elf` testfiles, and route them organically into execution via `ElfProgramLoader`.
* **Zfa Cross-Compiled E2E Evaluation**: Validating advanced Zfa semantics requires fully cross-compiled Zfa assembly ELFs executing cleanly through the top-level CPU simulation. Validating Zfa (Additional Floating-Point) instructions MUST instantiate a cross-compiled ELF payload utilizing `RiscVZfaIntegrationTest` to ensure organic memory load and `top_->Step()` execution. Instantiating raw `generic::Instruction` mocks is forbidden.

### Memory & System Architecture
* **Destructive RMW on MMIO Peripherals**: Agents MUST exempt MMIO address spaces from the 64-bit Read-Modify-Write (RMW) mandate. Unaligned RVVI writes must ensure explicit byte-lane masking support for device I/O regions.
* **FDT Magic Size Boundary**: Explicitly inject an exactly 4-byte DTB payload (containing only magic `0xd00dfeed`) to natively enforce the exact `if (dtb_size >= 4)` bounds condition in `riscv_dtb_loader.cc`.
* **Opaque OS Boot Registers**: E2E OS boot tests must execute authentic payloads that organically read `a0` and `a1` and write their values out to a verifiable memory address to prevent blindly executing `NOP` space.
* **Svadu Mock PTE Dependencies**: ALL legacy MMU tests MUST be proactively patched to include required mock permission bits (e.g., `| 0xC0`) to satisfy tightened architectural constraints rather than disabling the check.

### Plugins & Extensions
* **Dynamic Plugin Architecture for RVVI**: Build the RVVI plugin as a `cc_binary` target with `linkshared = True` in `mpact-riscv/riscv/BUILD`, linking against existing trace interfaces to satisfy external simulation environment bounds via `dlopen()`.
* **Zve32f Architecture Extraction**: Do not redefine floating-point instructions from scratch for CoralNPU M3. Extract the Zve32f instruction set strictly from the reference mpact-riscv `.isa` and `.bin_fmt` files.

### Structural Alignment & Memory Management
* **64-bit RMW Cycle Trap**: Implement thread-safety lock logic and atomize multi-register vector updates using strictly aligned 64-byte payload boolean flags (`fragment_index`, `is_last`).
* **Simulator Memory Mappers Lifecycle**: Guard mappers with `std::unique_ptr`. Update the base `MemoryInterface` pointer before initializing subsequent structures to prevent memory leaks and dangling mappers.
* **Struct Alignment & ABI**: Constrain `rvvi_trace_event_t` to strict 64-byte alignment with `#ifndef` guards. Prune contradictory legacy definitions. Use native assignments or `std::memcpy` instead of `absl::StrAppendFormat`.

## Mocked Exception Traps via Callback Interception
Artificially intercepting exceptions by overriding callbacks (e.g., `set_on_trap`) instead of allowing the hardware to organically route exceptions to the architectural trap handler vector and validating PC/CSR states is a testing illusion.

## Payload Validation & Extension Requirements
* **Malformed DTB Payload Obliteration**: The DTB loader must validate the FDT magic number (`0xd00dfeed`) and strictly bounds-check `.dtb` size natively in C++.
* **FP64 & Static Rounding Bypasses**: Dynamic unsupported width checks must explicitly trap `vsew=64`. Instructions requesting reserved static rounding modes (e.g., `rm = 101` or `110`) must raise Illegal Instruction exceptions.
* **Zfa Compilation Mocking**: Direct AST instantiation for Zfa validations is forbidden. Authentic E2E validations must cross-compile assembly strings into ELF payloads simulated via `rv64g_sim`.
* **CSR Architectural Validation**: When eradicating mocked `set_on_trap` callbacks, test authors must rely on natively evaluating architectural CSR registers (like `mcause` and `mepc`) populated by the default execution trap logic rather than intercepting the trap control flow manually.
