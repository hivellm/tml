## 1. Implementation
- [ ] 1.1 String ops: emit intrinsic dispatch for Str.split (strchr/loop), Str.join (strcat loop), Str.trim (leading/trailing whitespace pointers), Str.replace (strstr + memmove), Str.to_upper/to_lower (toupper/tolower loop), Str.parse_i64 (strtol), Str.parse_f64 (strtod)
- [ ] 1.2 I/O ops: emit intrinsic dispatch for File.open (open syscall, O_RDONLY/O_WRONLY/O_CREAT flags), File.read (read into heap buffer), File.write (write from buffer), File.close (close fd), stdin_read_line (fgets into heap buffer, strip newline)
- [ ] 1.3 Sync ops: emit intrinsic dispatch for Mutex.new (pthread_mutex_init into heap-allocated mutex), Mutex.lock (pthread_mutex_lock), Mutex.unlock (pthread_mutex_unlock), AtomicI64.fetch_add (__atomic_fetch_add), AtomicI64.load (__atomic_load_n), AtomicI64.store (__atomic_store_n)
- [ ] 1.4 Time ops: emit intrinsic dispatch for Time.now_ms (clock_gettime CLOCK_MONOTONIC → ms), Time.sleep_ms (nanosleep with ns = ms * 1_000_000)
- [ ] 1.5 Process ops: emit intrinsic dispatch for Process.exit (exit(code)), Process.env (getenv → Maybe[Str]), Process.args (argc/argv → List[Str] constructed at startup)
- [ ] 1.6 Integration test: a single test program exercises one intrinsic from each of the five groups and asserts the expected return value to confirm correct linkage and semantics

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
