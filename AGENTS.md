# MPACT-RISCV Lessons Learned
- **Fake Coverage via Unconditional SUCCEED()**: Tests must mathematically assert that the architectural state (e.g., PC advancement, `mcause`/`mtval` CSRs) remained valid and unaffected. Calling `SUCCEED()` without assertions is a severe violation of the "Happy-Path Padding" mandate.
- **Trace Ring Buffer Starvation**: The detached POSIX formatting thread can be starved by the OS (disk IO stall). The ISS SPSC yield loop must include a timeout boundary to explicitly crash and dump state rather than hanging CI infrastructure indefinitely.
- **RVA23 Trap Masking**: S-mode trap delegation and state-enable faults must be traced out-of-band to ensure full visibility during MMU panics, preventing silent deadlocks.
