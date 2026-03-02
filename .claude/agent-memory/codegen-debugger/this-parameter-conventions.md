# this/self Parameter Conventions in TML Codegen

## How `this` Gets Its Type

### Parser Level (parser_decl_impl.cpp:814-815)
Bare `this` (no explicit type) gets `NamedType("This")` automatically:
```cpp
auto this_type = make_box<Type>(
    Type{.kind = NamedType{TypePath{{"This"}, span}, {}, span}, .span = span});
```

### Codegen Level - Three Override Locations

#### 1. generate.cpp:1006-1011 (inline local impl codegen)
```cpp
if (param_name == "this" || param_name == "self") {
    if (is_primitive_impl && !param_is_mut) {
        param_type = impl_llvm_type;  // e.g., "i32"
    } else {
        param_type = "ptr";
    }
}
```

#### 2. impl.cpp:232-246 (gen_impl_method - library path)
Same logic: primitive immutable this -> by-value, everything else -> ptr.

#### 3. generate.cpp:1059-1067 (locals registration for body generation)
Same override applied when registering `locals_["this"]`.

## The Problem

All three locations unconditionally apply the "primitive by-value" rule based on:
- `is_primitive_impl`: whether the impl target is a primitive type
- `param_is_mut`: whether the param has `mut` keyword

They do NOT check:
- Whether the declared type is `ref This` (explicit pointer)
- Whether the behavior definition uses `this: ref This` vs bare `this`

## Convention

| Declaration | Primitive | Struct | Notes |
|---|---|---|---|
| `this` (bare) | by-value (i32) | by-ptr (ptr) | Library standard |
| `this: ref This` | by-ptr (ptr) | by-ptr (ptr) | Explicit reference |
| `mut this` | by-ptr (ptr) | by-ptr (ptr) | Mutation needs ptr |

The codegen currently treats rows 1 and 2 identically for primitives (both -> by-value), which is wrong.
