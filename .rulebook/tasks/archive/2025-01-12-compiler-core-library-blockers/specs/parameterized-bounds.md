# Spec: Parameterized Behavior Bounds

## Problem

The parser doesn't support behavior bounds with type parameters in generic constraints.

### Current Behavior

```tml
// This doesn't parse:
func collect[C: FromIterator[This::Item]](this) -> C {
    // ...
}

// Error: Expected ']' but found '['
```

### Expected Behavior

```tml
// Should parse and work:
behavior FromIterator[T] {
    func from_iter[I: Iterator](iter: I) -> This where I::Item = T
}

func collect[C: FromIterator[This::Item]](this) -> C {
    return C::from_iter(this)
}
```

## Root Cause

In `compiler/src/parser/parser_generics.cpp`, generic bounds parsing stops at identifier:

```cpp
// Current parsing:
// [T: SomeBehavior]  <- works
// [T: SomeBehavior[U]]  <- fails, doesn't expect '['
```

The parser expects:
```
generic_param := IDENT (':' bound)?
bound := IDENT
```

But should support:
```
bound := IDENT type_args?
type_args := '[' type (',' type)* ']'
```

## Fix

### Step 1: Update Grammar

```
generic_param := IDENT (':' bound_list)?
bound_list := bound ('+' bound)*
bound := IDENT type_args?
type_args := '[' type (',' type)* ']'
```

### Step 2: Update Parser

In `parser_generics.cpp`:

```cpp
auto Parser::parse_generic_bound() -> GenericBound {
    GenericBound bound;
    bound.name = expect(TokenKind::Ident).lexeme;

    // Check for type arguments
    if (check(TokenKind::LBracket)) {
        advance(); // consume '['
        bound.type_args = parse_type_arg_list();
        expect(TokenKind::RBracket);
    }

    return bound;
}
```

### Step 3: Update Type Checker

In `compiler/src/types/checker/generics.cpp`:

```cpp
auto TypeChecker::check_parameterized_bound(
    const GenericBound& bound,
    const TypePtr& concrete_type
) -> bool {
    // Find the behavior
    auto behavior = env_.get_behavior(bound.name);
    if (!behavior) return false;

    // Substitute type arguments
    TypeSubstitution subs;
    for (size_t i = 0; i < bound.type_args.size(); ++i) {
        subs[behavior->type_params[i]] = resolve_type(bound.type_args[i]);
    }

    // Check if concrete_type implements behavior with these args
    return check_impl_exists(concrete_type, behavior, subs);
}
```

## Example Usage

```tml
behavior FromIterator[T] {
    func from_iter[I: Iterator](iter: I) -> This where I::Item = T
}

impl FromIterator[I32] for List[I32] {
    pub func from_iter[I: Iterator](iter: I) -> List[I32] where I::Item = I32 {
        var list = List::new()
        loop {
            when iter.next() {
                Just(item) => list.push(item),
                Nothing => return list
            }
        }
    }
}

// Now this works:
func collect[C: FromIterator[This::Item]](this) -> C {
    return C::from_iter(this)
}

// Usage:
let numbers: List[I32] = (0 to 10).collect()
```

## Test Cases

```tml
@test
func test_parameterized_bound_parsing() -> I32 {
    // Just test that it parses
    behavior Container[T] {
        func contains(this, item: T) -> Bool
    }

    func find_in[C: Container[I32]](container: C, value: I32) -> Bool {
        return container.contains(value)
    }

    return 0
}

@test
func test_from_iterator_collect() -> I32 {
    let list: List[I32] = (0 to 5).collect()
    assert_eq(list.len(), 5, "collected list should have 5 elements")
    return 0
}
```
