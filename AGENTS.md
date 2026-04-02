# mpact-riscv Architectural Constraints

## Lessons Learned
- Ensure `cc_test` sizes in Bazel are explicitly bounded (`size="small"` or `size="large"`) to avoid OOM deadlocks.
- Do not mock authentic execution boundaries.
