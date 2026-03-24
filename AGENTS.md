# mpact-riscv Agent Instructions

## Lessons Learned

### Build Dependencies

- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.
- [FLAG: invalid] **RVA23 Monolithic Build**: Agents compiling bootable payloads (OpenSBI + Linux) must use an ephemeral out-of-tree workspace (e.g. `/tmp/riscv_os_build`). Only the final synthesized ELF artifact may be staged into the tracking tree via an exact `git add` to prevent repository bloat.
- [FLAG: stale] **Orchestration & Just-In-Time (JIT) Sync Atomicity**: Ensure local repository verification is executed before rebase. Bypassing redundant tests securely requires zero upstream divergence and an empty stash to guarantee the state isn't poisoned by previous detached stashes.
