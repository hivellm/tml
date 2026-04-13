## 1. Implementation
- [ ] 1.1 Implement `type_name[T]()` intrinsic in reflect.tml: emit a static null-terminated string constant in `.rodata` containing the mangled type name; the intrinsic loads and returns its pointer as Str
- [ ] 1.2 Implement `field_count[T]()` intrinsic: emit a static i64 constant in `.rodata` equal to the number of declared fields for struct type T; the intrinsic loads and returns it
- [ ] 1.3 Implement `field_name[T](i: I64)` intrinsic: emit a static array of `*const u8` string pointers in `.rodata`, one per field in declaration order; the intrinsic bounds-checks i and loads the i-th pointer as Str
- [ ] 1.4 Dynamic call emission in dyncall.tml: given a function pointer and a `List[DynArg]` (tagged union of I64/F64/Ptr), classify each arg by type, assign to the correct SysV AMD64 register (RDI/RSI/RDX/RCX/R8/R9 for integers, XMM0-7 for floats), push remaining args onto the stack, emit CALL through the pointer, read the return value from RAX/XMM0
- [ ] 1.5 Integration test: define a struct `Point { x: I64, y: I64 }`, assert `type_name[Point]() == "Point"`, `field_count[Point]() == 2`, `field_name[Point](0) == "x"`, `field_name[Point](1) == "y"`; also invoke a known function through a dynamic call and assert the return value

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
