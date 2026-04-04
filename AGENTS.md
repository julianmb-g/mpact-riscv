# mpact-riscv Architectural Constraints

## Lessons Learned & QA Mandates
* **API Boundary Drift & Override Modifiers**: When resolving `-Winconsistent-missing-override` warnings, you MUST verify that the method signature exactly matches the base class interface (e.g., in `riscv_debug_interface.h`). Blindly adding the `override` keyword when the base signature has drifted will result in fatal 'does not override any member functions' compilation failures.
* **Architectural Isolation**: Injecting external module-specific namespaces (like `coralnpu::rvvi`) directly into the generic `mpact-riscv` ISS violates architectural isolation boundaries and will cause compilation failures. Always enforce strict boundary separation.
* **Authentic Boot Sequence Validation:**: The simulator framework (e.g., `LinuxKernelBootloader`) MUST NOT actively write payload signatures like FDT magic numbers (`0xd00dfeed`) into memory. Authentic E2E validation dictates that test payloads and integration frameworks organically write/load these signatures into the memory map, allowing the cross-compiled boot sequence (e.g., `vmlinux.elf`) to natively probe and evaluate them.
* **Boot Sequence Test Fraud:**: Hardcoding magic numbers (e.g., FDT magic `0xd00dfeed`) directly into memory to bypass native OS boot sequence evaluation is Tier 1 Testing Fraud. The simulator must authentically parse the boot payload.
* **C++ Style Constraints**: Ensure C++ source files include their corresponding header file first, above other includes like absl, to adhere to the Google C++ Style Guide and prevent compilation failures or style violations.
* **Harness Regex Escaping**: When defining `--instrumentation_filter` in `harness.yaml`, always apply double backslash escaping (`\\^//`) to prevent Bazel unclosed group crashes during test execution.
* **LD Hex String Requirements**: When passing memory addresses to `ld` (e.g. via `-Ttext`), always use explicit hexadecimal string formats (e.g. `"0x20000000"` or `absl::StrCat("0x", absl::Hex(addr))`). Converting a base-10 number to a string using `std::to_string()` will cause GCC/ld to improperly interpret the decimal string as an un-prefixed hexadecimal value, leading to disastrous `PT_LOAD` segment misalignment.
* **Monolithic Boot Test Illusion:**: Placing a payload like `vmlinux.elf` into the repository without implementing an executing test that natively evaluates it is an illusion of coverage. Artifacts must be executed.
* **SRAM Mocking Fraud**: Unconditionally writing the FDT magic number directly to memory (e.g., in `riscv_boot.cc`) circumvents organic evaluation and is strictly forbidden.
* **Testing Fraud via Docstrings**: Maliciously renaming docstrings in SRAM tests to claim "Authentic SRAM RTL bounds" while continuing to use mocked Python loops/lists is Tier 1 Testing Fraud. Actual synthesized SRAM block responders MUST be instantiated.
* **Unauthorized Mutations**: Do not refactor pointer type punning (e.g., `reinterpret_cast`) into `std::memcpy` lambdas without explicit authorization. Such mutations circumvent the project's original C++ casting strategies and have been explicitly banned and reverted.
* **Watchdog Exception Integrity:** When implementing overarching C++ timeout exceptions natively, ensure that custom string headers (e.g., `trap_strings.h`) are correctly mapped into Bazel `deps` targets (e.g., `//sim:trap_strings`) to avoid compilation voids.
- Do not mock authentic execution boundaries.
- Ensure `cc_test` sizes in Bazel are explicitly bounded (`size="small"` or `size="large"`) to avoid OOM deadlocks.

# QA & Ledger Execution Mandates
*   **Data-Loss Audit Requirement**: Always perform a data-loss audit before ending the execution cycle.
*   **No Testing Illusions**: Do not mock physical TileLink/AXI boundaries with dictionaries. Instantiate genuine SystemVerilog responders.
*   **Test Runner Watchdogs**: Always use `pytest -x -n 0` to prevent watchdogs from terminating the execution silently.
* **Linker Evasion**: Enforce correct linker script usage (like linker.ld or vmlinux.ld) across integration tests natively instead of textually injecting test configurations.
