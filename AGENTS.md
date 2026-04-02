# mpact-riscv Architectural Constraints

## Lessons Learned
- Ensure `cc_test` sizes in Bazel are explicitly bounded (`size="small"` or `size="large"`) to avoid OOM deadlocks.
- Do not mock authentic execution boundaries.
- **SRAM Mocking Fraud**: Unconditionally writing the FDT magic number directly to memory (e.g., in `riscv_boot.cc`) circumvents organic evaluation and is strictly forbidden.
- **Testing Fraud via Docstrings**: Maliciously renaming docstrings in SRAM tests to claim "Authentic SRAM RTL bounds" while continuing to use mocked Python loops/lists is Tier 1 Testing Fraud. Actual synthesized SRAM block responders MUST be instantiated.
- **C++ Style Constraints**: Ensure C++ source files include their corresponding header file first, above other includes like absl, to adhere to the Google C++ Style Guide and prevent compilation failures or style violations.
- **Unauthorized Mutations**: Do not refactor pointer type punning (e.g., `reinterpret_cast`) into `std::memcpy` lambdas without explicit authorization. Such mutations circumvent the project's original C++ casting strategies and have been explicitly banned and reverted.
