# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Instruction Decoders:** Generated from `.isa` and `.bin_fmt` files, translating binary opcodes into executable instructions.
- **Instruction Set & Decoder Generation namespaces (`.isa` files):** When implementing new ISA profiles (e.g., RVA23) and modifying `.isa` decoder logic, explicitly qualify the `RV32` or `RV64` namespaces for semantic functions. Be careful when overriding opcodes from base `.isa` files; ensure the functions like `RiscVZextw` or `RiscVNot` exist in the expected namespace or the resulting decoder C++ files will fail to compile.
- **RISC-V Architecture**: Specifications for the unprivileged ISA, privileged architecture, scalable Vector (V) extension, and standard profiles (RVA23).
- **Vector Masking Separation:** Vector operations in `.isa` and `.bin_fmt` are duplicated to isolate masked (`vm=0`) and unmasked (`vm=1`) variants, resolving decoding ambiguity.
- **vill Trap Architectural Boundaries:** When modifying `vill` trap exception logic in the RTL, ensure only whole-register loads/stores, moves (`vmv<nr>r.v`), and scalar translations are whitelisted. Do not blindly whitelist all standard loads/stores, as this will bypass valid architectural fault boundaries. Tests and implementation must enforce this strictness.

### Testing Gotchas
- **Test Invariance & Environment Instantiation:** Tests for top-level binary simulators like `rva23u64_sim` must not be trivial `EXPECT_TRUE(true)` assertions. They must definitively link `_decoder`, `_top`, `_state`, `fp_state`, `vector_state` and allocate registers to mathematically prove the architectural environment initializes correctly without segfaulting.
- **Egregious Test Masking via Input Swapping:** When a test fails on specific instruction edge cases (like sign bit magnitude extraction in ZFA `FMINM.S`), you must not swap the inputs to "happy path" numbers that mask the failure. The underlying instruction decoder or logic must be fixed. Tests must strictly evaluate boundaries.

### Build Dependencies
- **Bazel Decoder Architecture Dependencies:** When implementing new instruction extensions (e.g., Zfa), ensure base architectural dependencies like `:riscv_g` are not accidentally removed from top-level `cc_library` targets in `riscv/BUILD`, as this causes silent missing symbol link errors.
- **Bazel Decoder Dependencies:** When a new `mpact_isa_decoder` macro relies on functionality, ensure the associated `cc_library` directly `deps` on the `.cc`/`.h` files implementing those semantic actions (like `riscv_bitmanip_instructions`), or compilation errors about unknown types (`Instruction`, `RegisterType`) will occur.
- **RVA23 Submodule Registration:** Any new missing extensions (like `Zawrs`) must include corresponding `.isa`, `.bin_fmt`, and `.cc/.h` definitions, registered directly inside the build definitions (e.g., `mpact-riscv/riscv/BUILD`) and integrated into the top-level target (e.g., `rva23u64.isa`).

### Git & Environment Management
- **JIT Synchronization Safety:** Successfully enforced the 'Zero-Trust Baseline Synchronization Safeguards' by executing `git status` and `git stash` prior to upstream synchronization. Confirmed pristine working tree, precluding stash conflicts.
- **JIT Synchronization Stash Resolution:** During Just-In-Time (JIT) upstream synchronization, if `git stash list` is empty (no stash was created), the stash popping step can be safely skipped and marked as complete without further action.
- **Upstream Sync Process:** Rebase against origin/main before executing tests to ensure a pristine state for the base ISS. Test evidence: 'bazel test //... --nocache_test_results' executed and passed (53/53 tests).
- **Zero-Trust Baseline Synchronization Safeguards (mpact-riscv):** Always mandate `git status` and `git stash` prior to invoking baseline extraction or rebasing logic to protect uncommitted orchestration state from accidental destruction.

### Miscellaneous
- **Linker Duplicate Symbols:** When adding support for disjoint instruction extensions (e.g. `Zfh` and `Vector`), do not reuse generic, un-namespaced helper functions (like `RV32VUnimplementedInstruction`) in different translation units. This causes `duplicate symbol` linker errors in the top-level `cc_binary` simulator. Either mark them `inline` in the header or uniquely namespace/prefix them (e.g. `RV32ZfhUnimplementedInstruction`).
- **Strict Aliasing Violation (reinterpret_cast):** Reinterpreting the address of an integer (`kCanonicalNaN`) as a float pointer (`*reinterpret_cast<T*>(&val)`) is a strict aliasing violation and invokes Undefined Behavior. Use `std::memcpy` or `std::bit_cast`.
- **RVA23 Ssnpm Pointer Masking Execution Flaw:** Pointer masking only applies to data loads/stores. Instruction fetches must fault if upper bits are not canonical to prevent severe security loopholes.
