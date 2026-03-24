# mpact-riscv Agent Instructions

## Lessons Learned

### Build Dependencies
- **Code Duplication & Refactoring (`rv32g_sim.cc` / `rv64g_sim.cc`)**: Both top-level simulators duplicate logic for `DataBuffer` trace formatting (`log_commits`). Ensure this logic is properly extracted to reusable utilities like `TraceFormatter` to maintain DRY principles and ease maintenance across architectural variants.

### Multi-Agent Artifact Review
[FLAG: stale] duplicate of global rule
- **Review Ledger Deduplication & Formatting**: The `REVIEW.md` artifact must strictly adhere to the `Quote-Impact-Action` format and `Tier 1/Tier 2` severity tiering.
