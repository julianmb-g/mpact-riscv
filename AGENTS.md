# mpact-riscv Architectural Constraints

## Lessons Learned
- Ensure `cc_test` sizes in Bazel are explicitly bounded (`size="small"` or `size="large"`) to avoid OOM deadlocks.
- Do not mock authentic execution boundaries.
# mpact-riscv ISS Constraints
* **RVA23 Monolithic OS Boot**: Boot validation must utilize pre-compiled OpenSBI/vmlinux payloads from an artifact path. From-scratch compilation of the monolithic OS is strictly forbidden.
* **RVVI Tracing**: Extract state hooks directly from `riscv_vector_instruction_helpers.h`. Event-driven CSR tracking must be evaluated at retirement.
