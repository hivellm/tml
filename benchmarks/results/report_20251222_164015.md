# TML Benchmark Report

**Generated:** 2025-12-22T16:40:13.483449

## System Information

| Property | Value |
|----------|-------|
| os | Windows |
| os_version | 10.0.19045 |
| machine | AMD64 |
| processor | AMD64 Family 25 Model 97 Stepping 2, AuthenticAMD |
| python_version | 3.13.9 |

## Results Summary

### Arithmetic

| Benchmark | Language | Build (ms) | Run (ms) | Binary (KB) | Status |
|-----------|----------|------------|----------|-------------|--------|
| arithmetic | C++ | 1007.4 | 41.2 | 142.5 | OK |
| arithmetic | Rust | 207.3 | 37.2 | 163.0 | OK |
| arithmetic | TML | 330.0 | 38.7 | 161.5 | OK |

## Performance Comparison

### arithmetic

**Build Time:**
  C++    ######################################## 1007.4ms
  Rust   ######## 207.3ms
  TML    ############# 330.0ms

**Run Time:**
  C++    ######################################## 41.2ms
  Rust   #################################### 37.2ms
  TML    ##################################### 38.7ms
