# 28. Type and Effect Checker

The minimal checker validates types and effects. Borrow checking is a separate pass.

## 1. Checker Phases

```
Source → Parser → AST
                   ↓
              Name Resolution
                   ↓
              Type Inference
                   ↓
              Effect Checking
                   ↓
              Validated AST (with types)
                   ↓
              Borrow Checking (separate)
                   ↓
              IR Generation
```

## 2. Name Resolution

### 2.1 Scope Structure

```tml
type Scope = {
    parent: Maybe[Heap[Scope]],
    bindings: Map[String, Binding],
    kind: ScopeKind,
}

type ScopeKind =
    | Module
    | Function
    | Block
    | Impl
    | Behavior

type Binding =
    | Variable(VarBinding)
    | Function(FuncBinding)
    | Type(TypeBinding)
    | Const(ConstBinding)
    | Module(ModBinding)
```

### 2.2 Resolution Rules

1. Look up in current scope
2. If not found, look in parent scope
3. For paths, resolve segment by segment
4. Imports bring names into scope

```tml
func resolve_path(scope: ref Scope, path: ref Path) -> Outcome[Binding, Error] {
    let mut current = resolve_first_segment(scope, path.segments[0])!

    for segment in path.segments[1..] {
        current = resolve_in_binding(current, segment)!
    }

    Ok(current)
}
```

### 2.3 Name Shadowing

- Variables can shadow outer variables
- Items cannot shadow other items in same scope
- Imports can be shadowed by local items

## 3. Type System

### 3.1 Type Representation

```tml
type Ty =
    // Primitives
    | Bool
    | Char
    | Int(IntTy)
    | Float(FloatTy)
    | Str
    | Unit
    | Never

    // Composite
    | Tuple(Vec[Ty])
    | Array(Heap[Ty], U64)
    | Slice(Heap[Ty])
    | Ref(RefKind, Heap[Ty])
    | Ptr(PtrKind, Heap[Ty])
    | Func(FuncTy)

    // User-defined
    | Adt(AdtId, Vec[Ty])  // struct/enum with generic args
    | Alias(AliasId, Vec[Ty])

    // Inference
    | Infer(InferVar)
    | Param(TypeParamId)
    | Error  // For error recovery

type IntTy = I8 | I16 | I32 | I64 | I128 | U8 | U16 | U32 | U64 | U128
type FloatTy = F32 | F64
type RefKind = Shared | Mutable
type PtrKind = Const | Mut

type FuncTy = {
    params: Vec[Ty],
    return_ty: Ty,
    effects: EffectSet,
}
```

### 3.2 Type Inference

Hindley-Milner with extensions:

```tml
type InferCtx = {
    substitutions: Map[InferVar, Ty],
    next_var: U32,
    constraints: Vec[Constraint],
}

type Constraint =
    | Eq(Ty, Ty, Span)           // T1 = T2
    | Subtype(Ty, Ty, Span)      // T1 <: T2 (for refs)
    | HasField(Ty, String, Ty)   // T has field F: U
    | Implements(Ty, BehaviorId) // T: Behavior
```

### 3.3 Unification

```tml
func unify(ctx: mut ref InferCtx, a: Ty, b: Ty, span: Span) -> Outcome[Unit, TypeError] {
    let a = apply_subst(ctx, a)
    let b = apply_subst(ctx, b)

    when (a, b) {
        // Same type
        (Bool, Bool) -> Ok(()),
        (Int(x), Int(y)) if x == y -> Ok(()),
        // ... other primitives

        // Inference variable
        (Infer(v), t) | (t, Infer(v)) -> {
            if occurs_check(v, t) then {
                Err(TypeError.InfiniteType(v, t, span))
            } else {
                ctx.substitutions.insert(v, t)
                Ok(())
            }
        },

        // Compound types
        (Tuple(as), Tuple(bs)) if as.len() == bs.len() -> {
            for (a, b) in as.zip(bs) {
                unify(ctx, a, b, span)!
            }
            Ok(())
        },

        (Adt(id1, args1), Adt(id2, args2)) if id1 == id2 -> {
            for (a, b) in args1.zip(args2) {
                unify(ctx, a, b, span)!
            }
            Ok(())
        },

        // Error
        _ -> Err(TypeError.Mismatch(a, b, span)),
    }
}
```

### 3.4 Type Checking Expressions

```tml
func check_expr(ctx: mut ref CheckCtx, expr: ref Expr, expected: Ty) -> Outcome[Ty, Error] {
    let actual = infer_expr(ctx, expr)!
    unify(ctx.infer, actual, expected, expr.span)!
    Ok(actual)
}

func infer_expr(ctx: mut ref CheckCtx, expr: ref Expr) -> Outcome[Ty, Error] {
    when expr.data {
        IntLit(lit) -> {
            if let Just(suffix) = lit.suffix then {
                Ok(Ty.Int(suffix))
            } else {
                // Default to I32, or infer from context
                Ok(Ty.Infer(ctx.fresh_var()))
            }
        },

        Ident(name) -> {
            let binding = ctx.resolve(name)!
            Ok(binding.ty())
        },

        Binary(b) -> {
            let left = infer_expr(ctx, b.left)!
            let right = infer_expr(ctx, b.right)!

            when b.op {
                Add | Sub | Mul | Div | Mod -> {
                    unify(ctx.infer, left, right, expr.span)!
                    Ok(left)
                },
                Eq | Ne | Lt | Le | Gt | Ge -> {
                    unify(ctx.infer, left, right, expr.span)!
                    Ok(Ty.Bool)
                },
                And | Or -> {
                    unify(ctx.infer, left, Ty.Bool, b.left.span)!
                    unify(ctx.infer, right, Ty.Bool, b.right.span)!
                    Ok(Ty.Bool)
                },
                // ... etc
            }
        },

        Call(c) -> {
            let func_ty = infer_expr(ctx, c.func)!

            when func_ty {
                Func(f) -> {
                    if c.args.len() != f.params.len() then {
                        return Err(Error.ArgCountMismatch(...))
                    }

                    for (arg, param_ty) in c.args.zip(f.params) {
                        check_expr(ctx, arg.value, param_ty)!
                    }

                    // Propagate effects
                    ctx.effects.extend(f.effects)

                    Ok(f.return_ty)
                },
                _ -> Err(Error.NotCallable(func_ty, expr.span)),
            }
        },

        If(i) -> {
            check_expr(ctx, i.condition, Ty.Bool)!
            let then_ty = infer_expr(ctx, i.then_branch)!

            if let Just(else_branch) = i.else_branch then {
                let else_ty = infer_expr(ctx, else_branch)!
                unify(ctx.infer, then_ty, else_ty, expr.span)!
                Ok(then_ty)
            } else {
                unify(ctx.infer, then_ty, Ty.Unit, expr.span)!
                Ok(Ty.Unit)
            }
        },

        // ... etc
    }
}
```

## 4. Effect System

### 4.1 Effect Representation

```tml
type Effect =
    | Pure         // No effects
    | IO           // File system, network, env
    | Async        // May suspend
    | Panic        // May panic
    | Alloc        // May allocate
    | Unsafe       // Unsafe operations

type EffectSet = {
    effects: Set[Effect],
}

impl EffectSet {
    func empty() -> This { This { effects: Set.new() } }
    func pure() -> This { This.empty() }
    func all() -> This { /* all effects */ }

    func contains(this: ref This, e: Effect) -> Bool
    func union(this: This, other: This) -> This
    func is_subset(this: ref This, other: ref This) -> Bool
}
```

### 4.2 Effect Checking

```tml
type EffectCtx = {
    allowed: EffectSet,      // Effects the current function may have
    actual: EffectSet,       // Effects actually used
}

func check_effects(ctx: mut ref CheckCtx, span: Span) -> Outcome[Unit, Error] {
    let disallowed = ctx.effects.actual.difference(ctx.effects.allowed)

    if not disallowed.is_empty() then {
        Err(Error.DisallowedEffects(disallowed, ctx.effects.allowed, span))
    } else {
        Ok(())
    }
}

func check_call_effects(
    ctx: mut ref CheckCtx,
    callee_effects: EffectSet,
    span: Span,
) -> Outcome[Unit, Error] {
    // Add callee effects to our actual effects
    ctx.effects.actual = ctx.effects.actual.union(callee_effects)

    // Unsafe is never inferred - must be explicit
    if callee_effects.contains(Effect.Unsafe) and
       not ctx.effects.allowed.contains(Effect.Unsafe) then {
        Err(Error.UnsafeInSafeContext(span))
    } else {
        Ok(())
    }
}
```

### 4.3 Effect Inference

```tml
func infer_func_effects(ctx: mut ref CheckCtx, func: ref FuncDef) -> EffectSet {
    let mut effects = EffectSet.empty()

    // Walk body and collect effects
    for call in func.body.calls() {
        effects = effects.union(call.effects)
    }

    // Certain operations have implicit effects
    if func.body.has_heap_alloc() then {
        effects = effects.union(EffectSet.from([Effect.Alloc]))
    }

    if func.body.has_panic() then {
        effects = effects.union(EffectSet.from([Effect.Panic]))
    }

    effects
}
```

### 4.4 Effect Polymorphism

```tml
// Function generic over effects
func map[T, U, E](
    list: List[T],
    f: func(T) -> U with E,
) -> List[U] with E {
    let mut result = List.new()
    for item in list {
        result.push(f(item))
    }
    result
}

// Inferred: map has effect E from f
```

## 5. Generic Instantiation

### 5.1 Monomorphization

```tml
func instantiate_generic(
    ctx: mut ref CheckCtx,
    generic: ref GenericDef,
    args: Vec[Ty],
) -> Outcome[Ty, Error] {
    if args.len() != generic.params.len() then {
        return Err(Error.GenericArgCountMismatch(...))
    }

    // Check bounds
    for (param, arg) in generic.params.zip(args) {
        for bound in param.bounds {
            if not implements(arg, bound) then {
                return Err(Error.BoundNotSatisfied(arg, bound, ...))
            }
        }
    }

    // Substitute type parameters
    let substituted = substitute(generic.ty, generic.params, args)
    Ok(substituted)
}
```

### 5.2 Behavior Implementation Check

```tml
func check_impl(ctx: mut ref CheckCtx, impl_block: ref ImplBlock) -> Outcome[Unit, Error] {
    let self_ty = resolve_type(ctx, impl_block.self_type)!

    if let Just(trait_ty) = impl_block.trait_type then {
        let trait_def = ctx.get_behavior(trait_ty)!

        // Check all required methods are implemented
        for method in trait_def.methods {
            if not impl_block.has_method(method.name) then {
                return Err(Error.MissingMethod(method.name, ...))
            }

            // Check signature compatibility
            let impl_method = impl_block.get_method(method.name)!
            check_method_signature(ctx, impl_method, method, self_ty)!
        }
    }

    // Check each method
    for item in impl_block.items {
        check_item(ctx, item)!
    }

    Ok(())
}
```

## 6. Error Messages

### 6.1 Type Error

```tml
type TypeError =
    | Mismatch {
        expected: Ty,
        actual: Ty,
        span: Span,
    }
    | InfiniteType {
        var: InferVar,
        ty: Ty,
        span: Span,
    }
    | NotCallable {
        ty: Ty,
        span: Span,
    }
    | FieldNotFound {
        ty: Ty,
        field: String,
        span: Span,
    }
    | MethodNotFound {
        ty: Ty,
        method: String,
        span: Span,
    }
    | BoundNotSatisfied {
        ty: Ty,
        bound: BehaviorId,
        span: Span,
    }
    | ArgCountMismatch {
        expected: U32,
        actual: U32,
        span: Span,
    }
```

### 6.2 Error Formatting

```
error[E0308]: type mismatch
  --> src/main.tml:10:5
   |
10 |     let x: I32 = "hello"
   |            ---   ^^^^^^^ expected `I32`, found `String`
   |            |
   |            expected due to this
   |
   = note: expected type `I32`
              found type `String`
```

## 7. Type Database

### 7.1 Structure

```tml
type TypeDb = {
    types: Map[TypeId, TypeDef],
    behaviors: Map[BehaviorId, BehaviorDef],
    impls: Map[(TypeId, BehaviorId), ImplId],
    funcs: Map[FuncId, FuncSig],
}

impl TypeDb {
    func get_type(this: ref This, id: TypeId) -> ref TypeDef
    func get_behavior(this: ref This, id: BehaviorId) -> ref BehaviorDef
    func find_impl(this: ref This, ty: Ty, behavior: BehaviorId) -> Maybe[ImplId]
    func get_method(this: ref This, ty: Ty, name: String) -> Maybe[FuncSig]
}
```

### 7.2 Prelude Types

```tml
func init_prelude(db: mut ref TypeDb) {
    // Primitives
    db.add_primitive(Bool)
    db.add_primitive(Char)
    db.add_primitive(I8)
    // ... etc

    // Built-in generics
    db.add_generic("Maybe", ["T"], [
        Variant("Just", [TypeParam("T")]),
        Variant("Nothing", []),
    ])

    db.add_generic("Outcome", ["T", "E"], [
        Variant("Ok", [TypeParam("T")]),
        Variant("Err", [TypeParam("E")]),
    ])

    // Built-in behaviors
    db.add_behavior("Eq", [
        Method("eq", [RefSelf, RefSelf], Bool),
    ])

    db.add_behavior("Ord", [
        Method("cmp", [RefSelf, RefSelf], Ordering),
    ])

    // ... etc
}
```

## 8. Incremental Checking

### 8.1 Dependency Tracking

```tml
type CheckCache = {
    file_types: Map[FileId, Vec[TypeId]],
    dependencies: Map[TypeId, Set[TypeId]>,
    results: Map[TypeId, CheckResult>,
}

impl CheckCache {
    func invalidate(this: mut ref This, file: FileId) {
        let types = this.file_types.get(file).unwrap_or_default()
        for ty in types {
            this.invalidate_type(ty)
        }
    }

    func invalidate_type(this: mut ref This, ty: TypeId) {
        this.results.remove(ty)

        // Invalidate dependents
        for dependent in this.dependents(ty) {
            this.invalidate_type(dependent)
        }
    }
}
```

### 8.2 Parallel Checking

```tml
func check_module_parallel(ctx: mut ref CheckCtx, module: ref Module) -> Outcome[Unit, Vec[Error]> {
    // Phase 1: Collect signatures (sequential)
    for item in module.items {
        collect_signature(ctx, item)!
    }

    // Phase 2: Check bodies (parallel)
    let errors = module.items
        .par_iter()
        .filter_map(do(item) {
            when check_item_body(ctx, item) {
                Ok(_) -> Nothing,
                Err(e) -> Just(e),
            }
        })
        .collect()

    if errors.is_empty() then Ok(()) else Err(errors)
}
```
