# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Instruction Decoders:** Generated from `.isa` and `.bin_fmt` files, translating binary opcodes into executable instructions.
- **Instruction Set & Decoder Generation namespaces (`.isa` files):** When implementing new ISA profiles (e.g., RVA23) and modifying `.isa` decoder logic, explicitly qualify the `RV32` or `RV64` namespaces for semantic functions. Be careful when overriding opcodes from base `.isa` files; ensure the functions like `RiscVZextw` or `RiscVNot` exist in the expected namespace or the resulting decoder C++ files will fail to compile.
- **Linker Duplicate Symbols:** When adding support for disjoint instruction extensions (e.g. `Zfh` and `Vector`), do not reuse generic, un-namespaced helper functions (like `RV32VUnimplementedInstruction`) in different translation units. This causes `duplicate symbol` linker errors in the top-level `cc_binary` simulator. Either mark them `inline` in the header or uniquely namespace/prefix them (e.g. `RV32ZfhUnimplementedInstruction`).
- **RISC-V Architecture:** Specifications for the unprivileged ISA, privileged architecture, scalable Vector (V) extension, and standard profiles (RVA23).
- **RVA23 Ssnpm Pointer Masking Execution Flaw:** Pointer masking only applies to data loads/stores. Instruction fetches must fault if upper bits are not canonical to prevent severe security loopholes.
- **Vector Masking Separation:** Vector operations in `.isa` and `.bin_fmt` are duplicated to isolate masked (`vm=0`) and unmasked (`vm=1`) variants, resolving decoding ambiguity.
- **Zawrs Polling Yield (mpause):** When simulating spin-wait loops (e.g., `WRS.NTO` in the `Zawrs` extension), use `std::this_thread::yield()` within the instruction semantic function (like `RiscVPause`) to yield the host simulator thread to the OS. This prevents the simulator from spinning at 100% CPU.

### Build Dependencies
- **Bazel Decoder Architecture Dependencies:** When implementing new instruction extensions (e.g., Zfa), ensure base architectural dependencies like `:riscv_g` are not accidentally removed from top-level `cc_library` targets in `riscv/BUILD`, as this causes silent missing symbol link errors.
- **Bazel Decoder Dependencies:** When a new `mpact_isa_decoder` macro relies on functionality, ensure the associated `cc_library` directly `deps` on the `.cc`/`.h` files implementing those semantic actions (like `riscv_bitmanip_instructions`), or compilation errors about unknown types (`Instruction`, `RegisterType`) will occur.
- **RVA23 Submodule Registration:** Any new missing extensions (like `Zawrs`) must include corresponding `.isa`, `.bin_fmt`, and `.cc/.h` definitions, registered directly inside the build definitions (e.g., `mpact-riscv/riscv/BUILD`) and integrated into the top-level target (e.g., `rva23u64.isa`).

### Testing Gotchas
- **Architectural Bounds Degradation:** When testing organic target execution, enforce precise architectural step bounds with strict equality assertions (e.g., `EXPECT_EQ(pc, entry_point + 4)`) instead of trivial inequality bounds checks (`EXPECT_NE(pc, entry_point)`), which provide no guarantee of correct instruction advancement.
- **Architectural Magnitude Masking:** Do not mask mathematical magnitude defects via test input swapping (e.g. FminmS). Tests must organically assert the true mathematical failure.
- **Egregious Test Masking via Input Swapping:** Never swap test inputs to mask an architectural bug. For example, if `FminmS_Magnitudes` fails with `a=-3.0, b=2.0`, do not swap them to `-2.0, 3.0` to force a pass. Mathematically fix the underlying architectural implementation in ZFA.
- **Egregious Test Masking via Input Swapping:** When a test fails on specific instruction edge cases (like sign bit magnitude extraction in ZFA `FMINM.S`), you must not swap the inputs to "happy path" numbers that mask the failure. The underlying instruction decoder or logic must be fixed. Tests must strictly evaluate boundaries.
- **MMIO Exemption Coverage:** Tests verifying MMIO cycle exemptions must execute memory accesses strictly inside the defined MMIO boundaries, not safely outside them.
- **RVVI SPSC Ring Buffer Initialization:** When implementing the RVVI telemetry hooks (e.g., `ClearTransientInstructionBuffer`), the `SpscRingBuffer` should strictly enforce explicit backpressure handling using `std::this_thread::yield()` when full, avoiding dropping trace packets while seamlessly integrating with C-API callbacks.
- **Sstc Timer Testing:** Always ensure STimeCmpCsr is fully covered with unit tests to prove architectural instantiation and callback triggering.
- **Strict Aliasing Violation (reinterpret_cast):** Reinterpreting the address of an integer (`kCanonicalNaN`) as a float pointer (`*reinterpret_cast<T*>(&val)`) is a strict aliasing violation and invokes Undefined Behavior. Use `std::memcpy` or `std::bit_cast`.
- **Supervisor-mode Timer Interrupts (Sstc):** When injecting decoupled hardware timer callbacks for Sstc, create a CSR class mapped to `stimecmp` (0x14D) derived from `RiscVSimpleCsr`. Pass the timer callback (e.g., `std::function`) to this class to cleanly intercept supervisor timer events without tightly coupling the CPU core state to the board-level clock.
- **Test Invariance & Environment Instantiation:** Tests for top-level binary simulators like `rva23u64_sim` must not be trivial `EXPECT_TRUE(true)` assertions. They must definitively link `_decoder`, `_top`, `_state`, `fp_state`, `vector_state` and allocate registers to mathematically prove the architectural environment initializes correctly without segfaulting.
- **Zero-Coverage MMIO Exemption Illusion:** When testing memory boundary conditions (like MMIO exemptions during unaligned RMW cycles), setting up the boundary is insufficient. The test MUST organically execute an access instruction *inside* the mapped boundary to actually trigger and prove the exemption logic. Do not pad tests with accesses outside the region.
- **ZFA Magnitude Integrity**: The `Fminm` and `Fmaxm` operations accurately evaluate mathematical magnitudes (`std::abs(a) < std::abs(b)`). Tests must authentically verify these architectural evaluations using inverted mathematical pairs (e.g., `-3.0, 2.0` -> `2.0`) rather than swapping inputs to mask the defect.

- **Zero-Coverage MMIO Exemption Illusion:** When verifying that unaligned Read-Modify-Write (RMW) cycles exempt memory-mapped I/O (MMIO) regions, the test must definitively execute an unaligned memory access *inside* the defined MMIO boundaries. Executing addresses outside the MMIO region (e.g., `mapper.Store(0x6, ...)` when MMIO is `0x1000-0x2000`) provides fraudulent coverage and completely misses the exemption logic.
## Lessons Learned

### Testing Gotchas
- **Zero-Coverage MMIO Exemption Illusion:** When verifying that unaligned Read-Modify-Write (RMW) cycles exempt memory-mapped I/O (MMIO) regions, the test must definitively execute an unaligned memory access *inside* the defined MMIO boundaries. Executing addresses outside the MMIO region provides fraudulent coverage and completely misses the exemption logic.
