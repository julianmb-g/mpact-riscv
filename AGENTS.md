
## Lessons Learned

### Architecture Quirks
- **Blind Unconditional Memory Leak**: RVVI memory mappers must be conditionally instantiated and guarded by `std::unique_ptr` to avoid permanent leakage during simulation termination.

### Cycle 6 Discovered Constraints & Lessons
- **Fake "Happy-Path" Duplication Padding**: Copy-pasting a test body and only swapping initialization variables introduces zero structural coverage and only bloats suite duration. Avoid mirroring test protocols (like `OpenSbiLinuxBootloaderTest` vs `TestLinuxBootProtocol`) where only initialization variables differ, as this adds zero structural coverage.

### Miscellaneous
- **Cosmetic RVVI Trivialization & Execution Bypassing**: Sending 'quit' immediately to interactive CLI wrappers (like `rvvi_cli_integration_test.cc`) completely bypasses RVVI trace production logic and formatting assertions.

### Testing Gotchas
- **Fake Coverage via Unconditional SUCCEED()**: Tests must mathematically assert that the architectural state (e.g., PC advancement, `mcause`/`mtval` CSRs) remained valid and unaffected. Calling `SUCCEED()` without assertions is a severe violation of the "Happy-Path Padding" mandate.
