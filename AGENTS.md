# mpact-riscv Agent Instructions

## Lessons Learned

### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Miscellaneous
- **Cosmetic RVVI Trivialization & Execution Bypassing**: Sending 'quit' immediately to interactive CLI wrappers (like `rvvi_cli_integration_test.cc`) completely bypasses RVVI trace production logic and formatting assertions.

### Testing Gotchas
- **Fake Coverage via Unconditional SUCCEED()**: Tests must mathematically assert that the architectural state (e.g., PC advancement, `mcause`/`mtval` CSRs) remained valid and unaffected. Calling `SUCCEED()` without assertions is a severe violation of the "Happy-Path Padding" mandate.
- **Rigid String Serialization Fragility**: When mathematically asserting structural trace mutations (e.g. executing interactive CLI wrappers), avoid hardcoding brittle, environmental hex bounds like specific PC addresses or register values. Organically assert trace output sequences (like opcode strings or generic register formatting) to prevent test failure upon compiler or libc updates.
