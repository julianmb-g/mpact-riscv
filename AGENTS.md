# mpact-riscv Agent Execution Knowledge

## Repository Role
An instruction set simulator for the standard RISC-V architecture, built upon the MPACT-Sim toolkit. Developed in C++ with custom `.isa` and `.bin_fmt` files, it provides the core processor state definitions and top-level executable simulators (like `rv32g_sim` and `rv64g_sim`).

## Lessons Learned
- Currently tracking the main branch. Ensure any features implemented are modular and adhere to the MPACT-Sim framework.
- Git constraints: Use HTTPS remotes only. Do not attempt to use `git remote set-url` to convert to SSH.