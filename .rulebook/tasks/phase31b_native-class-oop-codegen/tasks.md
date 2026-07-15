## 1. Implementation
- [ ] 1.1 Emit class struct layout: in mir_lower.tml, when lowering a ClassDecl, emit a struct type whose first field is the base class struct (or a vtable pointer I64 for root classes), followed by declared fields in order
- [ ] 1.2 Emit vtable constant: for each concrete class, emit a static I64 array named `<ClassName>__vtable` whose elements are function-pointer-cast values of the class's virtual methods in slot order; slots inherited from base class preserve the same index
- [ ] 1.3 Emit constructor chain: when lowering a class constructor in pipeline.tml, prepend a call to the base class constructor passing the object pointer before any derived-field initialisation instructions
- [ ] 1.4 Emit virtual call: lower VirtualCall MIR instruction to: load i64* from offset 0 of object (vtable ptr), GEP to slot index * 8, load function pointer, emit indirect call with object pointer as first argument
- [ ] 1.5 Emit instanceof check: lower InstanceOf MIR instruction to a comparison of the object's vtable pointer (load from offset 0) against the target class's vtable symbol address; result is I64 0 or 1
- [ ] 1.6 Test: define a base class Animal with virtual method speak() returning I64, derive Dog overriding speak() to return 42, instantiate Dog, call speak() via base pointer, assert result 42; also assert instanceof Dog returns 1 and instanceof Cat returns 0

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
