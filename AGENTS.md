# mpact-riscv Agent Instructions

## Lessons Learned & Orchestration Rules

### Core Execution Integrity
* **Cosmetic RVVI Trivialization Ban**: Tests must natively execute and format RVVI traces. Do not immediately send "quit" to interactive CLI wrappers to bypass trace generation logic.
* **64-bit RMW Cycle Trap**: Implement thread-safety lock logic and atomize multi-register vector updates using strictly aligned 64-byte payload boolean flags (`fragment_index`, `is_last`).
* **Isolated Execution Evaluation**: Asserting memory writes without checking the CPU orchestrator executes the bytes is testing fraud. Payload loading tests MUST instantiate the `RiscvTop` execution loop and step the CPU natively.
* **Test Pyramid & Execution Verification (MANDATE 3)**: New features MUST enforce a strict Test Pyramid: an executing ELF payload evaluated natively by the top-level simulator. Isolated string matching or individual instruction evals without hardware integration are flagged.

### Memory & System Architecture
* **Destructive RMW on MMIO Peripherals**: Agents MUST exempt MMIO address spaces from the 64-bit Read-Modify-Write (RMW) mandate. Unaligned RVVI writes must ensure explicit byte-lane masking support for device I/O regions.
* **FDT Magic Size Boundary**: Explicitly inject an exactly 4-byte DTB payload (containing only magic `0xd00dfeed`) to natively enforce the exact `if (dtb_size >= 4)` bounds condition in `riscv_dtb_loader.cc`.
* **Opaque OS Boot Registers**: E2E OS boot tests must execute authentic payloads that organically read `a0` and `a1` and write their values out to a verifiable memory address to prevent blindly executing `NOP` space.
* **Svadu Mock PTE Dependencies**: ALL legacy MMU tests MUST be proactively patched to include required mock permission bits (e.g., `| 0xC0`) to satisfy tightened architectural constraints rather than disabling the check.

### Plugins & Extensions
* **Dynamic Plugin Architecture for RVVI**: Build the RVVI plugin as a `cc_binary` target with `linkshared = True` in `mpact-riscv/riscv/BUILD`, linking against existing trace interfaces to satisfy external simulation environment bounds via `dlopen()`.
* **Zve32f Architecture Extraction**: Do not redefine floating-point instructions from scratch for CoralNPU M3. Extract the Zve32f instruction set strictly from the reference mpact-riscv `.isa` and `.bin_fmt` files.

### Structural Alignment & Memory Management
* **Simulator Memory Mappers Lifecycle**: Guard mappers with `std::unique_ptr`. Update the base `MemoryInterface` pointer before initializing subsequent structures to prevent memory leaks and dangling mappers.
* **Struct Alignment & ABI**: Constrain `rvvi_trace_event_t` to strict 64-byte alignment with `#ifndef` guards. Prune contradictory legacy definitions. Use native assignments or `std::memcpy` instead of `absl::StrAppendFormat`.

### Memory Map Boundaries & 64-bit RMW
* **64-bit Read-Modify-Write (RMW) Trap Testing**: Execution validation tests must verify that unaligned 64-bit stores (e.g., `sd`) that overlap an 8-byte cache block line boundary correctly trigger the internal `DoRmwStore` mechanism. This ensures thread-safety `rmw_mutex_` locks are exercised natively when concurrent host threads manipulate `FlatDemandMemory`. Do not rely on atomic instructions (`amoadd.d`) for testing basic RMW traps as these organically trap to `0x0` upon encountering unsupported bounds.
