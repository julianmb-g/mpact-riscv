# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Instruction Decoders**: Generated from `.isa` and `.bin_fmt` files, translating binary opcodes into executable instructions.
- **Instruction Set & Decoder Generation namespaces (`.isa` files)**: When implementing new ISA profiles (e.g., RVA23) and modifying `.isa` decoder logic, explicitly qualify the `RV32` or `RV64` namespaces for semantic functions. Be careful when overriding opcodes from base `.isa` files; ensure the functions like `RiscVZextw` or `RiscVNot` exist in the expected namespace or the resulting decoder C++ files will fail to compile.
- **RISC-V Architecture**: Specifications for the unprivileged ISA, privileged architecture, scalable Vector (V) extension, and standard profiles (RVA23).
- **Vector Masking Separation**: Vector operations in `.isa` and `.bin_fmt` are duplicated to isolate masked (`vm=0`) and unmasked (`vm=1`) variants, resolving decoding ambiguity.

### Build Dependencies
- **Bazel Decoder Architecture Dependencies**: When implementing new instruction extensions (e.g., Zfa), ensure base architectural dependencies like `:riscv_g` are not accidentally removed from top-level `cc_library` targets in `riscv/BUILD`, as this causes silent missing symbol link errors.
- **RVA23 Submodule Registration**: Any new missing extensions (like `Zawrs`) must include corresponding `.isa`, `.bin_fmt`, and `.cc/.h` definitions, registered directly inside the build definitions (e.g., `mpact-riscv/riscv/BUILD`) and integrated into the top-level target (e.g., `rva23u64.isa`).

### Git & Environment Management
- **Linker Duplicate Symbols**: When adding support for disjoint instruction extensions (e.g. `Zfh` and `Vector`), do not reuse generic, un-namespaced helper functions (like `RV32VUnimplementedInstruction`) in different translation units. This causes `duplicate symbol` linker errors in the top-level `cc_binary` simulator. Either mark them `inline` in the header or uniquely namespace/prefix them (e.g. `RV32ZfhUnimplementedInstruction`).

### Miscellaneous
- **Custom CSR Write Constraints (`satp`)**: When adding new CSRs like `satp` that require custom write constraints (e.g. ignoring writes with unsupported modes like Sv48), define them as a templated subclass of `RiscVSimpleCsr` directly inside `riscv_xstatus.h` (or similar header) rather than relying on inline validation. Ensure the overridden `Write` method completely ignores the operation if the attempted `MODE` value is unsupported, leaving the entire register unmodified.
- **RVA23 Privilege Enforcement**: `cbo.zero` must strictly enforce PTE permissions to prevent U-mode privilege escalation. Pointer masking (Ssnpm) must only apply to data accesses, not instruction fetches (to preserve CFI).
- **RVA23 Ssnpm Pointer Masking Execution Flaw**: Pointer masking only applies to data loads/stores. Instruction fetches must fault if upper bits are not canonical to prevent severe security loopholes.
- [FLAG: stale] **Zawrs Polling Yield (mpause)**: When simulating spin-wait loops (e.g., `WRS.NTO` in the `Zawrs` extension), use `std::this_thread::yield()` within the instruction semantic function (now `RiscVWrsNto`) to yield the host simulator thread to the OS. This prevents the simulator from spinning at 100% CPU.

### Testing Gotchas
- **Egregious Test Masking via Input Swapping**: Never swap test inputs to mask an architectural bug. For example, if `FminmS_Magnitudes` fails with `a=-3.0, b=2.0`, do not swap them to `-2.0, 3.0` to force a pass. Mathematically fix the underlying architectural implementation in ZFA. Tests must strictly evaluate boundaries.
- **MMU Contextless Trap Assertions**: When testing MMU fault handling (e.g., `mmu_sv39_test`), explicitly verify the downstream architectural state by asserting `mcause` and `mtval` CSRs to ensure exceptions were correctly recorded, avoiding reliance on arbitrary boolean mocks.
- **MMU Instantiation & State Dependencies**: When scaffolding memory management units like `MmuSv39` that perform active page walks, do not construct them with only `MemoryInterface`. They must be explicitly instantiated with `RiscVState*` to access architectural CSRs like `satp` and `mstatus`. The associated Bazel targets (both `cc_library` and `_test`) must strictly depend on `//riscv:riscv_state` to prevent missing symbol linkage errors.
- **Negative PTE Fault Tests**: Distinct MMU page fault scenarios (like Store to Read-Only vs Invalid PTE) must be cleanly separated into distinct, targeted tests rather than bundled into a single negative test case. Each test must explicitly assert `mcause` and `mtval`.
- [FLAG: stale] **Organic Polling Verification**: When testing thread-yielding functions like `Zawrs` spin-wait loops (`RiscVWrsNto`), do not use boolean mock assertions (`EXPECT_TRUE(true)`). Prove execution organically by spawning a concurrent thread to set a flag, allowing the main test thread to repeatedly execute the yield loop. Assert that the loop increments and execution time scales correctly without CPU lockups.
- **RVA23 Zfa Testing**: When implementing Zfa Fminm/Fmaxm tests, ensure the tests explicitly assert both the canonical minimum/maximum magnitude values according to RISC-V Zfa specification.
- **Test Invariance & Environment Instantiation**: Tests for top-level binary simulators like `rva23u64_sim` must not be trivial `EXPECT_TRUE(true)` assertions. They must definitively link `_decoder`, `_top`, `_state`, `fp_state`, `vector_state` and allocate registers to mathematically prove the architectural environment initializes correctly without segfaulting.
- **Zawrs Emulation strictness**: Spin-wait loops using instructions like `WRS.NTO` must organically call `std::this_thread::yield()` to prevent CI CPU lockups during out-of-band proxy test execution.
- **Zawrs Polling Yield Tautology**: When refactoring brittle wall-clock timer tests (`sleep_for`) for thread yielding functions, do not create tautological tests that simply verify a background thread successfully sets a flag without evaluating the yield behavior. You MUST enforce an architectural evaluation boundary by organically comparing the yielded loop iterations against a tight spin loop (`nop`), but using a deterministic state-machine target (e.g., `bg_state < 5000000`) instead of a timer to guarantee stability across environments.

- **Instruction Operands Evaluation**: When implementing semantic functions, extract register values using `mpact::sim::generic::GetInstructionSource<uint64_t>(inst, index)` rather than calling `.AsUint64(0)` directly on sources. In unit tests, inject source inputs via `new mpact::sim::generic::ImmediateOperand<uint64_t>(val)` instead of allocating `DataBuffer` directly for `Source(0)`.
- **Decoder Configuration Registration**: When introducing a new ISA extension like `Zicbo`, ensure the `.isa` and `.bin_fmt` files define the precise bit patterns (`opcode == 0b0001111`) and are strictly added to the `includes = [...]` blocks in `BUILD` for the top-level processors (e.g., `rva23u64_isa`, `rvm23_isa`).
- **Memory Interface Initialization**: When generating test targets simulating memory accesses (e.g. `cbo.zero`), ensure `RiscVState` is properly initialized with a valid `MemoryInterface` (like `FlatDemandMemory`) instead of `nullptr`, otherwise memory operations will silently segfault.
