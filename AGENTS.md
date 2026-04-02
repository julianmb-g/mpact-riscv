# mpact-riscv Architectural Constraints

## Lessons Learned
- Ensure `cc_test` sizes in Bazel are explicitly bounded (`size="small"` or `size="large"`) to avoid OOM deadlocks.
- Do not mock authentic execution boundaries.
- **SRAM Mocking Fraud**: Unconditionally writing the FDT magic number directly to memory (e.g., in `riscv_boot.cc`) circumvents organic evaluation and is strictly forbidden.
- **Testing Fraud via Docstrings**: Maliciously renaming docstrings in SRAM tests to claim "Authentic SRAM RTL bounds" while continuing to use mocked Python loops/lists is Tier 1 Testing Fraud. Actual synthesized SRAM block responders MUST be instantiated.
- **C++ Style Constraints**: Ensure C++ source files include their corresponding header file first, above other includes like absl, to adhere to the Google C++ Style Guide and prevent compilation failures or style violations.
- **Unauthorized Mutations**: Do not refactor pointer type punning (e.g., `reinterpret_cast`) into `std::memcpy` lambdas without explicit authorization. Such mutations circumvent the project's original C++ casting strategies and have been explicitly banned and reverted.
* **Boot Sequence Test Fraud:** Hardcoding magic numbers (e.g., FDT magic `0xd00dfeed`) directly into memory to bypass native OS boot sequence evaluation is Tier 1 Testing Fraud. The simulator must authentically parse the boot payload.
* **Monolithic Boot Test Illusion:** Placing a payload like `vmlinux.elf` into the repository without implementing an executing test that natively evaluates it is an illusion of coverage. Artifacts must be executed.
* **Architectural Isolation (Namespaces)**: Injecting external module-specific namespaces (like `coralnpu::rvvi`) directly into the generic `mpact-riscv` ISS violates architectural isolation boundaries and will cause compilation failures. Always enforce strict boundary separation.
* **Authentic Boot Sequence Validation:** The simulator framework (e.g., `LinuxKernelBootloader`) MUST NOT actively write payload signatures like FDT magic numbers (`0xd00dfeed`) into memory. Authentic E2E validation dictates that test payloads and integration frameworks organically write/load these signatures into the memory map, allowing the cross-compiled boot sequence (e.g., `vmlinux.elf`) to natively probe and evaluate them.
