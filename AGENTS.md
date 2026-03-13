# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture & Integration
- **Bazel Decoder Architecture Dependencies:** When implementing new instruction extensions (e.g., Zfa), ensure base architectural dependencies like `:riscv_g` are not accidentally removed from top-level `cc_library` targets in `riscv/BUILD`, as this causes silent missing symbol link errors.
- **Bazel Decoder Dependencies:** When a new `mpact_isa_decoder` macro relies on functionality, ensure the associated `cc_library` directly `deps` on the `.cc`/`.h` files implementing those semantic actions (like `riscv_bitmanip_instructions`), or compilation errors about unknown types (`Instruction`, `RegisterType`) will occur.
- **Instruction Decoders:** Generated from `.isa` and `.bin_fmt` files, translating binary opcodes into executable instructions.
- **Instruction Set & Decoder Generation namespaces (`.isa` files):** When implementing new ISA profiles (e.g., RVA23) and modifying `.isa` decoder logic, explicitly qualify the `RV32` or `RV64` namespaces for semantic functions. Be careful when overriding opcodes from base `.isa` files; ensure the functions like `RiscVZextw` or `RiscVNot` exist in the expected namespace or the resulting decoder C++ files will fail to compile.
- **Linker Duplicate Symbols:** When adding support for disjoint instruction extensions (e.g. `Zfh` and `Vector`), do not reuse generic, un-namespaced helper functions (like `RV32VUnimplementedInstruction`) in different translation units. This causes `duplicate symbol` linker errors in the top-level `cc_binary` simulator. Either mark them `inline` in the header or uniquely namespace/prefix them (e.g. `RV32ZfhUnimplementedInstruction`).
- **RISC-V Architecture**: Specifications for the unprivileged ISA, privileged architecture, scalable Vector (V) extension, and standard profiles (RVA23).
- **Vector Masking Separation:** Vector operations in `.isa` and `.bin_fmt` are duplicated to isolate masked (`vm=0`) and unmasked (`vm=1`) variants, resolving decoding ambiguity.

### Git & Environment Management
- **RVA23 Submodule Registration:** Any new missing extensions (like `Zawrs`) must include corresponding `.isa`, `.bin_fmt`, and `.cc/.h` definitions, registered directly inside the build definitions (e.g., `mpact-riscv/riscv/BUILD`) and integrated into the top-level target (e.g., `rva23u64.isa`).
- **Zero-Trust Baseline Synchronization Safeguards (mpact-riscv):** Always mandate `git status` and `git stash` prior to invoking baseline extraction or rebasing logic to protect uncommitted orchestration state from accidental destruction.

### Testing & Verification
- **FMINM.S ZFA Input Swapping:** The ZFA Minimum Magnitude test was corrupted by swapping inputs `-2.0f, 3.0f` to `-3.0f, 2.0f`. This masked a sign-bit handling defect in `RiscVFMinmS`. Tests must strictly evaluate the negative minimum magnitude boundaries.
- **Test Invariance & Environment Instantiation:** Tests for top-level binary simulators like `rva23u64_sim` must not be trivial `EXPECT_TRUE(true)` assertions. They must definitively link `_decoder`, `_top`, `_state`, `fp_state`, `vector_state` and allocate registers to mathematically prove the architectural environment initializes correctly without segfaulting.

### Miscellaneous
- **vill Trap Architectural Boundaries:** When modifying `vill` trap exception logic in the RTL, ensure only whole-register loads/stores, moves (`vmv<nr>r.v`), and scalar translations are whitelisted. Do not blindly whitelist all standard loads/stores, as this will bypass valid architectural fault boundaries.
