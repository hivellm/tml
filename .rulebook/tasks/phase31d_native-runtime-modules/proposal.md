# Proposal: phase31d_native-runtime-modules

## Why
The native backend currently has only 48 of the ~500 C runtime function
signatures declared in `runtime_decls.tml`. Every call into the TML
standard library that touches threads, networking, crypto, filesystem,
or regex must fall through to the LLVM backend because the native
backend cannot emit a call to an undeclared symbol. As a result the
native backend is unusable for any real-world program that exceeds
simple arithmetic. Declaring the full set of C runtime signatures
unblocks all five remaining stdlib modules and is a prerequisite for
every subsequent phase that tests native output against the full suite.

## What Changes
- `compiler-tml/src/codegen/runtime_decls.tml` is extended with five
  new declaration groups, each matching the exact C ABI signature used
  by the TML runtime:
  - Concurrency: `thread_create`, `thread_join`, `thread_detach`,
    `atomic_load_i64`, `atomic_store_i64`, `atomic_fetch_add_i64`,
    `atomic_compare_exchange_i64`, `channel_create`, `channel_send`,
    `channel_recv`, `channel_close`.
  - Networking: `socket`, `bind`, `listen`, `accept`, `connect`,
    `send`, `recv`, `setsockopt`, `getsockopt`, `getaddrinfo`,
    `freeaddrinfo`, `inet_ntop`, `htons`, `ntohs`, `close_socket`.
  - Crypto: `sha256_init`, `sha256_update`, `sha256_final`,
    `aes128_encrypt_block`, `aes128_decrypt_block`,
    `aes256_encrypt_block`, `aes256_decrypt_block`.
  - Filesystem: `fs_stat`, `fs_mkdir`, `fs_readdir`, `fs_readdir_next`,
    `fs_readdir_close`, `fs_unlink`, `fs_rename`.
  - Regex: `regex_compile`, `regex_free`, `regex_match`,
    `regex_find`, `regex_replace`.
- Each declaration uses the existing `@extern("c")` annotation pattern
  and matches the C header types already in `runtime/include/`.

## Impact
- Affected specs: native-backend/runtime-decls
- Affected code: compiler-tml/src/codegen/runtime_decls.tml
- Breaking change: NO
- User benefit: Stdlib modules for threads, networking, crypto, filesystem, and regex can be called from natively compiled code without falling back to LLVM.
