## 1. Borrowed handle system
- [ ] 1.1 Add is_borrowed flag to handle metadata
- [ ] 1.2 Add alloc_borrowed_handle function
- [ ] 1.3 Update free to not free borrowed handles
- [ ] 1.4 Update get_json_value for borrowed handles

## 2. Wire into accessor FFI
- [ ] 2.1 Change object_get to return borrowed handle
- [ ] 2.2 Change array_get to return borrowed handle
- [ ] 2.3 Build and run JSON tests

## 3. Benchmark gate
- [ ] 3.1 Run json_bench — Field Access under 3000 ns

## 4. Tail (mandatory)
- [ ] 4.1 Update docs with new numbers
- [ ] 4.2 Test parse + access + free lifecycle
- [ ] 4.3 Run tests and confirm pass
