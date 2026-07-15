## 1. Implementation
- [ ] 1.1 Concurrency: declare thread_create, thread_join, thread_detach, atomic_load_i64, atomic_store_i64, atomic_fetch_add_i64, atomic_compare_exchange_i64, channel_create, channel_send, channel_recv, channel_close with @extern("c") in runtime_decls.tml
- [ ] 1.2 Networking: declare socket, bind, listen, accept, connect, send, recv, setsockopt, getsockopt, getaddrinfo, freeaddrinfo, inet_ntop, htons, ntohs, close_socket with correct I32/RawPtr/I64 parameter types
- [ ] 1.3 Crypto: declare sha256_init, sha256_update, sha256_final, aes128_encrypt_block, aes128_decrypt_block, aes256_encrypt_block, aes256_decrypt_block matching runtime/include/crypto.h signatures
- [ ] 1.4 Filesystem: declare fs_stat, fs_mkdir, fs_readdir, fs_readdir_next, fs_readdir_close, fs_unlink, fs_rename with RawPtr/Str/I32 parameter types matching runtime/include/fs.h
- [ ] 1.5 Regex: declare regex_compile, regex_free, regex_match, regex_find, regex_replace matching runtime/include/regex.h signatures
- [ ] 1.6 Verify: for each new declaration, confirm the TML parameter types match the C header types by cross-referencing runtime/include/ headers; compile a small test program that calls one function from each group

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
