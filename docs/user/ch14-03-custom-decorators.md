# Custom Decorators

When the built-in decorators do not cover your use case, you can define your own. A custom
decorator is a compile-time function that receives the item it annotates as a syntax tree,
transforms it, and returns a new syntax tree. The compiler substitutes the returned tree in
place of the original item.

## Defining a Decorator

Use the `decorator` keyword to introduce a custom decorator. The decorator function receives
one argument — the annotated item wrapped in a `DecoratorTarget` — and returns `DecoratorTarget`:

```tml
decorator func log_calls(target: DecoratorTarget) -> DecoratorTarget {
    // Inspect and transform target
    return target
}
```

Apply it the same way as any built-in decorator:

```tml
@log_calls
func greet(name: Str) {
    println("Hello, {}!", name)
}
```

## The `DecoratorTarget` Type

`DecoratorTarget` is a discriminated union over the kinds of items a decorator can annotate:

```tml
enum DecoratorTarget {
    Func(FuncDecl),
    Type(TypeDecl),
    Enum(EnumDecl),
    Var(VarDecl),
    Mod(ModDecl),
}
```

You pattern-match on `target` to determine what was annotated and extract the relevant
declaration. If the decorator is applied to an unsupported item kind, you can either
panic with a useful message or return the target unchanged.

## Code Generation with `quote` and `splice`

The `quote!` macro captures TML source as a syntax tree. The `splice!` macro inserts an
expression into a `quote!` block:

```tml
decorator func log_calls(target: DecoratorTarget) -> DecoratorTarget {
    when target {
        DecoratorTarget::Func(decl) => {
            let name = decl.name
            let body = decl.body

            // Wrap the original body with logging
            let new_body = quote! {
                println("[ENTER] {}", splice!(name))
                let result = { splice!(body) }
                println("[EXIT]  {}", splice!(name))
                result
            }

            return DecoratorTarget::Func(FuncDecl {
                name: decl.name,
                params: decl.params,
                return_type: decl.return_type,
                body: new_body,
            })
        },
        _ => panic("@log_calls can only be applied to functions"),
    }
}
```

With this decorator defined, the following annotated function:

```tml
@log_calls
func compute(x: I32) -> I32 {
    return x * x
}
```

Compiles as if you had written:

```tml
func compute(x: I32) -> I32 {
    println("[ENTER] compute")
    let result = { return x * x }
    println("[EXIT]  compute")
    result
}
```

## Decorator Arguments

To accept arguments, define additional parameters on the decorator function. Arguments
must be compile-time constants (string literals, integer literals, or boolean literals):

```tml
decorator func retry(target: DecoratorTarget, times: I32) -> DecoratorTarget {
    when target {
        DecoratorTarget::Func(decl) => {
            let body = decl.body
            let new_body = quote! {
                var _attempts = 0
                var _result = { splice!(body) }
                loop {
                    if _result.is_ok() or _attempts >= splice!(times) { break }
                    _attempts += 1
                    _result = { splice!(body) }
                }
                _result
            }
            return DecoratorTarget::Func(FuncDecl { body: new_body, ..decl })
        },
        _ => panic("@retry can only be applied to functions"),
    }
}
```

Call it with the argument in parentheses:

```tml
@retry(3)
func fetch_data(url: Str) -> Outcome[Response, NetworkError] {
    // ...
}
```

Named arguments are also supported by declaring parameters by name:

```tml
decorator func timeout(target: DecoratorTarget, millis: I32 = 5000) -> DecoratorTarget {
    // ...
}

@timeout(millis = 1000)
func slow_query() -> Outcome[Row, DbError] {
    // ...
}
```

## Restricting Which Items a Decorator May Annotate

Use `DecoratorTarget` pattern matching to enforce which item kinds are accepted and emit
a clear compile error for unsupported uses:

```tml
decorator func singleton(target: DecoratorTarget) -> DecoratorTarget {
    when target {
        DecoratorTarget::Type(decl) => {
            // Generate singleton accessor
            let type_name = decl.name
            let new_items = quote! {
                splice!(decl)

                extend splice!(type_name) {
                    var INSTANCE: Maybe[splice!(type_name)] = Nothing

                    pub func instance() -> ref splice!(type_name) {
                        if INSTANCE.is_nothing() {
                            INSTANCE = Just(splice!(type_name)::default())
                        }
                        return ref INSTANCE.unwrap()
                    }
                }
            }
            return DecoratorTarget::Mod(ModDecl { body: new_items })
        },
        _ => panic(
            "@singleton can only be applied to struct types, not to " +
            target.kind_name()
        ),
    }
}
```

## Custom Derive Behaviors

Custom decorators integrate with `@derive`. If you implement the `Derivable` behavior for
your decorator and register it with the compiler, users can name it inside `@derive(...)`:

```tml
decorator func Displayable(target: DecoratorTarget) -> DecoratorTarget {
    when target {
        DecoratorTarget::Type(decl) => {
            let type_name = decl.name
            let field_fmts = decl.fields
                .map(do(f) quote! { "{}: {}", splice!(f.name), self.splice!(f.name) })
                .join(", ")

            let to_string_body = quote! {
                return splice!(type_name).to_string() + " { " + splice!(field_fmts) + " }"
            }

            return DecoratorTarget::Mod(ModDecl {
                body: quote! {
                    splice!(decl)
                    extend splice!(type_name) : Display {
                        func to_string(this) -> Str {
                            splice!(to_string_body)
                        }
                    }
                },
            })
        },
        _ => panic("@Displayable requires a struct type"),
    }
}
```

After registration, users write:

```tml
@derive(Displayable)
type Coordinate { lat: F64, lon: F64 }
```

## Where Custom Decorators Are Defined

Custom decorators must be defined in a module that is compiled before the modules that use
them. Placing them in a dedicated `decorators.tml` file or in a `macros` sub-module
and importing that module is the standard pattern:

```tml
// decorators/trace.tml
pub decorator func trace(target: DecoratorTarget) -> DecoratorTarget {
    // ...
}
```

```tml
// main.tml
use decorators::trace

@trace
func important_function() {
    // ...
}
```

## Limitations

- Decorator functions are evaluated at compile time. They cannot call runtime functions,
  perform I/O, or allocate heap memory.
- A decorator cannot inspect the internals of the types used in a function's signature
  beyond their names. For reflection at runtime, see the reflection module.
- Circular decorator definitions — a decorator that decorates itself — are a compile error.
- Decorators cannot be applied to local `let`/`var` bindings; only to module-level items.

## See Also

- [Built-in Decorators](ch14-01-builtin-decorators.md) — the full reference for all
  compiler-provided decorators
- [Derive Macros](ch14-02-derive.md) — the built-in `@derive` behaviors and their generated
  code
- [Behaviors](ch05-00-behaviors.md) — the behavior system that `@derive` implements
