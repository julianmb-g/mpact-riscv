# mpact-riscv Submodule Knowledge

## Lessons Learned
- **Asynchronous Clock Simulation (Sstc):** Hardware timer interrupts must be simulated organically by stepping the clock until the threshold is passed. Do not eviscerate the simulation with static constants (e.g. `simulated_clock = 5`) to force an instant trigger. Also, ensure `mideleg` is configured (e.g., `mideleg.STIP = 1`) and `csrw stimecmp` is used.
- **Mideleg Trap Routing Constraints:** When asserting Supervisor-level interrupts (like `STIP`), tests MUST explicitly configure `mideleg` to delegate the interrupt (e.g. `mideleg.STIP = 1`). Otherwise, the interrupt natively traps to M-mode instead of S-mode, routing the PC to `mtvec` (default 0) instead of the configured `stvec`.
- **Interrupt Retirement Boundaries:** When writing organic architectural tests that assert interrupts, be aware that the simulator core `CheckForInterrupt()` evaluates and takes the trap at the exact instruction retirement boundary of the triggering instruction within the same `Step(1)` call.
