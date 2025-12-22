#include "tml/types/env.hpp"

namespace tml::types {

// Scope implementation
Scope::Scope(std::shared_ptr<Scope> parent) : parent_(std::move(parent)) {}

void Scope::define(const std::string& name, TypePtr type, bool is_mutable, SourceSpan span) {
    symbols_[name] = Symbol{name, std::move(type), is_mutable, span};
}

auto Scope::lookup(const std::string& name) const -> std::optional<Symbol> {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->lookup(name);
    }
    return std::nullopt;
}

auto Scope::parent() const -> std::shared_ptr<Scope> {
    return parent_;
}

// TypeEnv implementation
TypeEnv::TypeEnv() {
    current_scope_ = std::make_shared<Scope>();
    init_builtins();
}

void TypeEnv::init_builtins() {
    // Builtin types
    builtins_["I8"] = make_primitive(PrimitiveKind::I8);
    builtins_["I16"] = make_primitive(PrimitiveKind::I16);
    builtins_["I32"] = make_primitive(PrimitiveKind::I32);
    builtins_["I64"] = make_primitive(PrimitiveKind::I64);
    builtins_["I128"] = make_primitive(PrimitiveKind::I128);
    builtins_["U8"] = make_primitive(PrimitiveKind::U8);
    builtins_["U16"] = make_primitive(PrimitiveKind::U16);
    builtins_["U32"] = make_primitive(PrimitiveKind::U32);
    builtins_["U64"] = make_primitive(PrimitiveKind::U64);
    builtins_["U128"] = make_primitive(PrimitiveKind::U128);
    builtins_["F32"] = make_primitive(PrimitiveKind::F32);
    builtins_["F64"] = make_primitive(PrimitiveKind::F64);
    builtins_["Bool"] = make_primitive(PrimitiveKind::Bool);
    builtins_["Char"] = make_primitive(PrimitiveKind::Char);
    builtins_["Str"] = make_primitive(PrimitiveKind::Str);
    builtins_["Unit"] = make_unit();

    // Builtin functions
    SourceSpan builtin_span{};

    // print(s: Str) -> Unit
    functions_["print"] = FuncSig{
        "print",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // println(s: Str) -> Unit
    functions_["println"] = FuncSig{
        "println",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // print_i32(n: I32) -> Unit
    functions_["print_i32"] = FuncSig{
        "print_i32",
        {make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // print_bool(b: Bool) -> Unit
    functions_["print_bool"] = FuncSig{
        "print_bool",
        {make_primitive(PrimitiveKind::Bool)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // Memory allocation functions
    auto ptr_type = make_ref(make_primitive(PrimitiveKind::I32), true);

    // alloc(size: I32) -> ptr
    functions_["alloc"] = FuncSig{
        "alloc",
        {make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // dealloc(ptr) -> Unit
    functions_["dealloc"] = FuncSig{
        "dealloc",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // read_i32(ptr) -> I32
    functions_["read_i32"] = FuncSig{
        "read_i32",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // write_i32(ptr, value: I32) -> Unit
    functions_["write_i32"] = FuncSig{
        "write_i32",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ptr_offset(ptr, offset: I32) -> ptr
    functions_["ptr_offset"] = FuncSig{
        "ptr_offset",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // ============ ATOMIC OPERATIONS (Thread-Safe) ============

    // atomic_load(ptr) -> I32
    functions_["atomic_load"] = FuncSig{
        "atomic_load",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_store(ptr, value: I32) -> Unit
    functions_["atomic_store"] = FuncSig{
        "atomic_store",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // atomic_add(ptr, value: I32) -> I32
    functions_["atomic_add"] = FuncSig{
        "atomic_add",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_sub(ptr, value: I32) -> I32
    functions_["atomic_sub"] = FuncSig{
        "atomic_sub",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_exchange(ptr, value: I32) -> I32
    functions_["atomic_exchange"] = FuncSig{
        "atomic_exchange",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_cas(ptr, expected: I32, desired: I32) -> Bool
    functions_["atomic_cas"] = FuncSig{
        "atomic_cas",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // atomic_cas_val(ptr, expected: I32, desired: I32) -> I32
    functions_["atomic_cas_val"] = FuncSig{
        "atomic_cas_val",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_and(ptr, value: I32) -> I32
    functions_["atomic_and"] = FuncSig{
        "atomic_and",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_or(ptr, value: I32) -> I32
    functions_["atomic_or"] = FuncSig{
        "atomic_or",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // fence() -> Unit
    functions_["fence"] = FuncSig{
        "fence",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // fence_acquire() -> Unit
    functions_["fence_acquire"] = FuncSig{
        "fence_acquire",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // fence_release() -> Unit
    functions_["fence_release"] = FuncSig{
        "fence_release",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ SPINLOCK PRIMITIVES ============

    // spin_lock(lock_ptr) -> Unit
    functions_["spin_lock"] = FuncSig{
        "spin_lock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // spin_unlock(lock_ptr) -> Unit
    functions_["spin_unlock"] = FuncSig{
        "spin_unlock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // spin_trylock(lock_ptr) -> Bool
    functions_["spin_trylock"] = FuncSig{
        "spin_trylock",
        {ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // ============ THREADING PRIMITIVES ============

    // thread_spawn(func_ptr, arg_ptr) -> thread_handle (ptr)
    functions_["thread_spawn"] = FuncSig{
        "thread_spawn",
        {ptr_type, ptr_type},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // thread_join(handle) -> Unit
    functions_["thread_join"] = FuncSig{
        "thread_join",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_yield() -> Unit
    functions_["thread_yield"] = FuncSig{
        "thread_yield",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_sleep(ms: I32) -> Unit
    functions_["thread_sleep"] = FuncSig{
        "thread_sleep",
        {make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_id() -> I32
    functions_["thread_id"] = FuncSig{
        "thread_id",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ GO-STYLE CHANNELS ============

    // channel_create() -> channel_ptr
    functions_["channel_create"] = FuncSig{
        "channel_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // channel_send(ch, value: I32) -> Bool (true if success)
    functions_["channel_send"] = FuncSig{
        "channel_send",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_recv(ch) -> I32
    functions_["channel_recv"] = FuncSig{
        "channel_recv",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // channel_try_send(ch, value: I32) -> Bool
    functions_["channel_try_send"] = FuncSig{
        "channel_try_send",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_try_recv(ch, out_ptr) -> Bool
    functions_["channel_try_recv"] = FuncSig{
        "channel_try_recv",
        {ptr_type, ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_close(ch) -> Unit
    functions_["channel_close"] = FuncSig{
        "channel_close",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // channel_destroy(ch) -> Unit
    functions_["channel_destroy"] = FuncSig{
        "channel_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // channel_len(ch) -> I32
    functions_["channel_len"] = FuncSig{
        "channel_len",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ MUTEX PRIMITIVES ============

    // mutex_create() -> mutex_ptr
    functions_["mutex_create"] = FuncSig{
        "mutex_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // mutex_lock(m) -> Unit
    functions_["mutex_lock"] = FuncSig{
        "mutex_lock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // mutex_unlock(m) -> Unit
    functions_["mutex_unlock"] = FuncSig{
        "mutex_unlock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // mutex_try_lock(m) -> Bool
    functions_["mutex_try_lock"] = FuncSig{
        "mutex_try_lock",
        {ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // mutex_destroy(m) -> Unit
    functions_["mutex_destroy"] = FuncSig{
        "mutex_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ WAITGROUP (GO-STYLE) ============

    // waitgroup_create() -> wg_ptr
    functions_["waitgroup_create"] = FuncSig{
        "waitgroup_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // waitgroup_add(wg, delta: I32) -> Unit
    functions_["waitgroup_add"] = FuncSig{
        "waitgroup_add",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_done(wg) -> Unit
    functions_["waitgroup_done"] = FuncSig{
        "waitgroup_done",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_wait(wg) -> Unit
    functions_["waitgroup_wait"] = FuncSig{
        "waitgroup_wait",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_destroy(wg) -> Unit
    functions_["waitgroup_destroy"] = FuncSig{
        "waitgroup_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };
}

void TypeEnv::define_struct(StructDef def) {
    structs_[def.name] = std::move(def);
}

void TypeEnv::define_enum(EnumDef def) {
    enums_[def.name] = std::move(def);
}

void TypeEnv::define_behavior(BehaviorDef def) {
    behaviors_[def.name] = std::move(def);
}

void TypeEnv::define_func(FuncSig sig) {
    functions_[sig.name] = std::move(sig);
}

void TypeEnv::define_type_alias(const std::string& name, TypePtr type) {
    type_aliases_[name] = std::move(type);
}

auto TypeEnv::lookup_struct(const std::string& name) const -> std::optional<StructDef> {
    auto it = structs_.find(name);
    if (it != structs_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_enum(const std::string& name) const -> std::optional<EnumDef> {
    auto it = enums_.find(name);
    if (it != enums_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef> {
    auto it = behaviors_.find(name);
    if (it != behaviors_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_func(const std::string& name) const -> std::optional<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_type_alias(const std::string& name) const -> std::optional<TypePtr> {
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end()) return it->second;
    return std::nullopt;
}

void TypeEnv::push_scope() {
    current_scope_ = std::make_shared<Scope>(current_scope_);
}

void TypeEnv::pop_scope() {
    if (current_scope_->parent()) {
        current_scope_ = current_scope_->parent();
    }
}

auto TypeEnv::current_scope() -> std::shared_ptr<Scope> {
    return current_scope_;
}

auto TypeEnv::fresh_type_var() -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = TypeVar{type_var_counter_++, std::nullopt};
    return type;
}

void TypeEnv::unify(TypePtr a, TypePtr b) {
    if (a->is<TypeVar>()) {
        substitutions_[a->as<TypeVar>().id] = b;
    } else if (b->is<TypeVar>()) {
        substitutions_[b->as<TypeVar>().id] = a;
    }
}

auto TypeEnv::resolve(TypePtr type) -> TypePtr {
    if (!type) return type;

    if (type->is<TypeVar>()) {
        auto id = type->as<TypeVar>().id;
        auto it = substitutions_.find(id);
        if (it != substitutions_.end()) {
            return resolve(it->second);
        }
    }
    return type;
}

auto TypeEnv::builtin_types() const -> const std::unordered_map<std::string, TypePtr>& {
    return builtins_;
}

} // namespace tml::types
