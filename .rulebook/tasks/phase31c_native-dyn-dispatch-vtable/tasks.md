## 1. Implementation
- [ ] 1.1 Emit vtable struct layout: in emit_method.tml, define a helper emit_vtable_type(behavior) that returns a struct type with fields [drop_fn: I64, size: I64, align: I64, method_0: I64, ...] where method slots follow in behavior declaration order
- [ ] 1.2 Emit vtable constant: for each (ConcreteType, Behavior) pair seen at a MakeDyn site, emit a static constant named `<Type>_as_<Behavior>__vtable` whose method slots hold the addresses of the concrete type's implementations cast to I64; drop_fn holds the type's drop glue address, size and align hold the concrete type's known values
- [ ] 1.3 Emit make_dyn: lower MakeDyn MIR instruction to allocate a two-field fat pointer struct {data_ptr: I64, vtable_ptr: I64} on the stack, store &value into data_ptr and &<Type>_as_<Behavior>__vtable into vtable_ptr, return the fat pointer address
- [ ] 1.4 Emit dyn call: lower DynCall MIR instruction to load vtable_ptr from fat_ptr+8, GEP to slot offset (method_index * 8 + 24), load the I64 function pointer, cast to the method's concrete signature type, emit an indirect call prepending data_ptr as the first argument
- [ ] 1.5 Test dyn Iterator: define a struct Counter implementing Iterator[I64] with next() returning Just(42) then Nothing; box it as dyn Iterator[I64]; call next() twice; assert first returns Just(42) and second returns Nothing
- [ ] 1.5 Test dyn Display: define a struct Point implementing Display with to_string() returning "Point"; box as dyn Display; call to_string(); assert result equals "Point"

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
