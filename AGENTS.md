
## Lessons Learned

### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Cycle 6 Discovered Constraints & Lessons
- **Fake "Happy-Path" Duplication Padding**: Copy-pasting a test body and only swapping initialization variables introduces zero structural coverage and only bloats suite duration. Avoid mirroring test protocols (like `OpenSbiLinuxBootloaderTest` vs `TestLinuxBootProtocol`) where only initialization variables differ, as this adds zero structural coverage.

### Miscellaneous
- **Cosmetic RVVI Trivialization & Execution Bypassing**: Sending 'quit' immediately to interactive CLI wrappers (like `rvvi_cli_integration_test.cc`) completely bypasses RVVI trace production logic and formatting assertions.

### Testing Gotchas
- **Fake Coverage via Unconditional SUCCEED()**: Tests must mathematically assert that the architectural state (e.g., PC advancement, `mcause`/`mtval` CSRs) remained valid and unaffected. Calling `SUCCEED()` without assertions is a severe violation of the "Happy-Path Padding" mandate.
- **Interactive Execution Bypassing Fraud**: When eradicating cosmetic execution bypassing in CLI integration tests (such as feeding only `quit` to an interactive wrapper), the interactive wrapper can still be utilized by feeding an organic execution sequence like `step 10\nreg info\nquit\n`. This allows the test to organically load the ELF payload, execute the pipeline, and mathematically assert chronological trace mutations by evaluating the standard output for correct decoding and register mutations, fully enforcing timeline verification.
- **Rigid String Serialization Fragility**: When mathematically asserting structural trace mutations (e.g. executing interactive CLI wrappers), avoid hardcoding brittle, environmental hex bounds like specific PC addresses or register values. Organically assert trace output sequences (like opcode strings or generic register formatting) to prevent test failure upon compiler or libc updates.
