TML_MODULE("compiler")

//! # Reflection Intrinsic Error Explanations
//!
//! Error codes R001-R005 for compile-time reflection intrinsic errors.
//! These intrinsics allow TML code to inspect type structure at compile time.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_reflection_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"R001", R"EX(
Reflection field index out of bounds [R001]

A compile-time reflection intrinsic (`field_name`, `field_type_id`,
`field_offset`) was called with a field index that exceeds the number of
fields in the type.

Example of erroneous code:

    type Point { x: F64, y: F64 }    // 2 fields (index 0, 1)

    // Compile-time reflection
    let name = field_name[Point](2)   // error: index 2 out of bounds (2 fields)
    let off  = field_offset[Point](5) // error: index 5 out of bounds (2 fields)

How to fix:

Use `field_count[T]()` to get the number of fields before iterating:

    let count = field_count[Point]()  // returns 2
    loop (i: USize = 0; i < count; i += 1) {
        let name = field_name[Point](i)
    }

Field indices are 0-based. Valid range: 0 to `field_count[T]() - 1`.

Related intrinsics:
- `field_count[T]()` — number of fields in T
- `field_name[T](index)` — name of field at index
- `field_type_id[T](index)` — type ID of field at index
- `field_offset[T](index)` — byte offset of field at index
)EX"},

        {"R002", R"EX(
Reflection method index out of bounds [R002]

A compile-time reflection intrinsic (`method_name`, `is_virtual`,
`is_override`) was called with a method index that exceeds the number of
methods in the class.

Example of erroneous code:

    class Animal {
        func speak(this) -> Str { return "..." }
    }
    // Animal has 1 method (index 0)

    let name = method_name[Animal](5)  // error: index 5 out of bounds

How to fix:

Use `method_count[T]()` to get the number of methods before iterating:

    let count = method_count[Animal]()
    loop (i: I64 = 0; i < count; i += 1) {
        let name = method_name[Animal](i)
    }

Note: Currently `method_name` and similar intrinsics silently return null
when the index is out of bounds. R002 is reserved for when explicit error
reporting is added to these sites.
)EX"},

        {"R003", R"EX(
Reflection interface/behavior bounds error [R003]

A compile-time reflection intrinsic was called with an index that exceeds
the number of implemented behaviors (interfaces) for a type.

This code is reserved for future use when explicit error reporting is added
to behavior-reflection intrinsics.

How to fix:

Use the behavior count intrinsic to guard the access before calling
behavior-reflection functions with an index.
)EX"},

        {"R004", R"EX(
Reflection metadata not available [R004]

A compile-time reflection intrinsic was called on a type that does not
have the required metadata available at compile time.

Common causes:

1. The type is a primitive (I32, F64, etc.) and does not have struct metadata
2. The type is a generic parameter that has not been monomorphized yet
3. The type was defined in a module that was not fully analyzed

How to fix:

Reflection intrinsics work on:
- Struct types (field_count, field_name, field_offset, field_type_id)
- Class types (method_count, method_name, is_virtual, is_abstract)
- Enum types (variant_count)
- All types (type_name, type_id)

Ensure the type passed to the reflection intrinsic is a concrete struct,
class, or enum — not a primitive or unresolved generic.
)EX"},

        {"R005", R"EX(
Reflection type resolution failure [R005]

A compile-time reflection intrinsic could not resolve the type argument
to a concrete type. This happens when the type parameter cannot be
determined at compile time.

Common causes:

1. The type argument is a generic parameter (not yet monomorphized)
2. The type name is ambiguous or not in scope
3. The type was defined with a conditional compilation block that was
   excluded for the current target

How to fix:

Ensure the type argument to reflection intrinsics is a concrete, fully
resolved type:

    // Works: concrete type
    let count = field_count[MyStruct]()

    // May fail: unresolved generic
    func inspect[T]() {
        let count = field_count[T]()  // T must be concrete at call site
    }
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
