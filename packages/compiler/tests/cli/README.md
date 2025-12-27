# CLI Integration Tests

This directory contains integration tests for the TML CLI.

## Running Tests

### Test --out-dir Parameter

Tests that the `--out-dir` parameter correctly places build artifacts in custom directories:

```bash
bash packages/compiler/tests/cli/test_out_dir.sh
```

This test validates:
1. Building executables with custom output directory
2. Building static libraries with custom output directory
3. Building libraries with headers in custom output directory

## Adding New Tests

1. Create a test TML file (e.g., `test_feature.tml`)
2. Create a test script (e.g., `test_feature.sh`)
3. Make the script executable: `chmod +x test_feature.sh`
4. Run the test: `bash test_feature.sh`

## Test Files

- `test_out_dir.tml` - Sample TML code for testing --out-dir
- `test_out_dir.sh` - Integration test for --out-dir parameter
