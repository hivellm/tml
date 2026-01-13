# Specification: Devirtualization Pass

## Overview

Devirtualization converts virtual method calls to direct calls when the concrete type is statically known. This eliminates vtable lookup overhead and enables further optimizations like inlining.

## Cases for Devirtualization

### Case 1: Exact Type from Constructor

When an object is created with `Class::new()`, its type is exactly known:

```tml
let dog = Dog::new()
dog.speak()  // Type is exactly Dog, not a subclass
```

**Analysis**: Track type through SSA value. If value comes from constructor call, type is exact.

### Case 2: Sealed Classes

Sealed classes cannot be extended:

```tml
sealed class Leaf extends Node {
    override func process(this) { ... }
}

func work(node: Leaf) {
    node.process()  // Must be exactly Leaf
}
```

**Analysis**: Check class definition for `sealed` modifier.

### Case 3: Final Methods

Final methods cannot be overridden:

```tml
class Base {
    final func calculate(this) -> I32 { ... }
}

class Derived extends Base { }

func use(b: Base) {
    b.calculate()  // Can devirtualize - method is final
}
```

**Analysis**: Check method definition for `final` modifier or implicit final (sealed class).

### Case 4: Single Implementation (CHA)

When class hierarchy analysis shows only one concrete implementation:

```tml
abstract class Animal {
    abstract func speak(this)
}

// Only one concrete class in entire program
class Dog extends Animal {
    override func speak(this) { print("woof") }
}

func make_sound(a: Animal) {
    a.speak()  // Can devirtualize if Dog is only implementation
}
```

**Analysis**: Query class hierarchy for all concrete subclasses.

## Implementation

### Data Flow

```
1. Type Checker builds ClassHierarchy
                ↓
2. HIR Builder annotates expressions with type info
                ↓
3. Devirtualization pass analyzes call sites
                ↓
4. Replace virtual calls with direct calls
                ↓
5. Inlining pass can now inline devirtualized calls
```

### HIR Representation

Before devirtualization:
```
VirtualCall {
    receiver: %dog,
    method: "speak",
    vtable_index: 0,
    args: []
}
```

After devirtualization:
```
DirectCall {
    func: "Dog_speak",
    args: [%dog]
}
```

### Algorithm

```cpp
void DevirtualizationPass::run(HIRFunction& func) {
    for (auto& block : func.blocks()) {
        for (auto& inst : block.instructions()) {
            if (auto* vcall = inst.as<VirtualCall>()) {
                if (auto resolved = try_devirtualize(vcall)) {
                    replace_with_direct_call(vcall, *resolved);
                    stats_.devirtualized++;
                }
            }
        }
    }
}

std::optional<std::string> try_devirtualize(VirtualCall* vcall) {
    // Case 1: Exact type from constructor
    if (auto exact_type = get_exact_type(vcall->receiver)) {
        return get_method_impl(*exact_type, vcall->method);
    }

    // Case 2: Sealed class
    auto static_type = vcall->receiver_type;
    if (hierarchy_.is_sealed(static_type)) {
        return get_method_impl(static_type, vcall->method);
    }

    // Case 3: Final method
    if (hierarchy_.is_final_method(static_type, vcall->method)) {
        return get_method_impl(static_type, vcall->method);
    }

    // Case 4: Single implementation
    auto impls = hierarchy_.get_all_implementations(static_type, vcall->method);
    if (impls.size() == 1) {
        return impls[0];
    }

    return std::nullopt;
}
```

## Statistics

Track for `--time` output:
- Total virtual call sites
- Devirtualized calls (count and percentage)
- Breakdown by case (exact type, sealed, final, CHA)

## Testing

### Test Cases

1. **exact_type_devirt.tml**: Constructor followed by method call
2. **sealed_class_devirt.tml**: Sealed class method calls
3. **final_method_devirt.tml**: Final method on non-sealed class
4. **single_impl_devirt.tml**: CHA-based devirtualization
5. **no_devirt_polymorphic.tml**: Ensure polymorphic calls stay virtual
6. **inheritance_chain.tml**: A extends B extends C, test each level

### Verification

Compare generated LLVM IR before/after devirtualization:
- Before: `call void %func_ptr(ptr %obj)` (indirect)
- After: `call void @tml_Dog_speak(ptr %obj)` (direct)
