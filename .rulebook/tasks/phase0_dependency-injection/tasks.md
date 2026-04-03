# Tasks: Dependency Injection — CMMV-Inspired Singleton Registry + NestJS Decorators

**Status**: Planning. 0% (0/48).
**Approach**: CMMV-style (singleton registry, no heavy IoC) + NestJS decorators
**Target API**:
```tml
@Service("user")
type UserService {
    db: SqliteConnection
}

impl UserService {
    pub func new(db: SqliteConnection) -> UserService {
        return UserService { db: db }
    }
    pub func find_all(this) -> Str { return "[]" }
}

@Controller("/users")
type UserController {
    service: ref UserService
}

// Module — explicit flat wiring, no IoC magic
func main() -> I32 {
    let db = SqliteConnection::open_in_memory().unwrap()
    let user_svc = UserService::new(db)

    var app = App::new()
    let module = Module::new()
        .service("user", ref user_svc)
        .controller(UserController { service: ref user_svc })
    app.bootstrap(module)
    app.listen(3000)
    return 0
}
```

## CMMV vs NestJS Approach

```
CMMV (what we follow):
1. Services are singletons — created once, stored in flat registry
2. No IoC container — direct references, explicit wiring
3. Module = flat list of {controllers, providers} — no recursion
4. ServiceRegistry = global HashMap[Str, I64] for named service access
5. Zero overhead — no proxy objects, no scope resolution, no reflection

NestJS (what we DON'T do):
1. Heavy IoC container with reflect-metadata
2. Automatic constructor injection
3. Recursive module resolution
4. Proxy-based lazy injection
```

## Phase 1: Service Registry — Singleton Store (CMMV-style) (8 items)

### Core registry
- [ ] 1.1 `di/registry.tml` — `ServiceRegistry` type: flat HashMap[Str, I64] mapping token → instance ptr
- [ ] 1.2 `ServiceRegistry::new()` — creates empty registry
- [ ] 1.3 `ServiceRegistry::register(token, ptr)` — stores a service instance by name
- [ ] 1.4 `ServiceRegistry::get(token) -> Maybe[I64]` — retrieves service ptr by name
- [ ] 1.5 `ServiceRegistry::has(token) -> Bool` — checks if service exists
- [ ] 1.6 `ServiceRegistry::list() -> Str` — returns comma-separated list of registered tokens

### Global singleton access
- [ ] 1.7 `di/global.tml` — module-level `ServiceRegistry` instance (singleton pattern)
- [ ] 1.8 `register_service(name, ptr)`, `get_service(name) -> I64` — convenience functions

## Phase 2: @Service Decorator + Module Builder (8 items)

### @Service decorator (CMMV-style — just metadata, no magic)
- [ ] 2.1 Parser: `@Service("token")` on type declarations → stores token as decorator arg
- [ ] 2.2 `di/service.tml` — `ServiceDef` type: token, type_name, instance_ptr
- [ ] 2.3 `di/service.tml` — `ServiceDef::new(token, ptr)` — wraps a pre-built instance

### Module builder (CMMV-style — flat, explicit wiring)
- [ ] 2.4 `di/module.tml` — `Module` type with fluent builder
- [ ] 2.5 `Module::new()` → `Module::service(token, ptr)` → `Module::controller(ctrl)` → `Module::build()`
- [ ] 2.6 `Module::build()` — registers all services in ServiceRegistry, returns controller list
- [ ] 2.7 `Module::import(other_module)` — merges another module's services into this one (submodules)

### Tests
- [ ] 2.8 Test: Module::new().service("db", ptr).service("user", ptr).build() registers both

## Phase 3: HTTP Integration — App.bootstrap(Module) (8 items)

### Wiring to HTTP
- [ ] 3.1 `App::bootstrap(module)` — calls module.build(), then registers controllers with router
- [ ] 3.2 Controller methods receive services via `this.field` (direct ref, no proxy)
- [ ] 3.3 `http/context.tml` — `RequestContext` for per-request data (not per-request DI — CMMV-style)
- [ ] 3.4 Auto-register routes from @Controller + @Get/@Post methods during bootstrap

### ServiceRegistry access in handlers
- [ ] 3.5 `get_service("user")` available globally — any handler can look up a service by token
- [ ] 3.6 Type-safe wrapper: `get_user_service() -> ref UserService` (generated or manual)

### Tests
- [ ] 3.7 Test: App.bootstrap(module) → GET /users calls UserController which uses UserService
- [ ] 3.8 Test: get_service("db") returns the same ptr registered at startup

## Phase 4: Contract-Driven Generation (CMMV's killer feature) (8 items)

### Contracts — define once, generate everything
- [ ] 4.1 `di/contract.tml` — `Contract` type: entity_name, fields[], endpoints[]
- [ ] 4.2 `Contract::entity(name)` → `.field("id", "INTEGER", primary=true)` → `.field("name", "TEXT")`
- [ ] 4.3 `Contract::crud()` — auto-adds GET /, GET /:id, POST /, PUT /:id, DELETE /:id endpoints
- [ ] 4.4 `contract_generate_controller(contract)` → generates controller type + route handlers
- [ ] 4.5 `contract_generate_service(contract)` → generates service with find_all/find_one/create/update/delete
- [ ] 4.6 `contract_generate_entity(contract)` → generates EntityMeta for db::orm

### Module integration
- [ ] 4.7 `Module::contract(contract)` — auto-generates + registers controller + service from contract
- [ ] 4.8 Tests: Contract::entity("task").field("title","TEXT").crud() → full CRUD API working

## Phase 5: Custom Providers — useValue, useFactory (5 items)

### Advanced patterns (same as NestJS but simpler)
- [ ] 5.1 `ServiceRegistry::register_value(token, i64_value)` — raw value (config, port number)
- [ ] 5.2 `ServiceRegistry::register_factory(token, factory_fn)` — lazy, called on first get()
- [ ] 5.3 `ServiceRegistry::alias(new_token, existing_token)` — alias lookup
- [ ] 5.4 Config provider: `register_config(key, value)` + `get_config(key)` — typed config access

### Tests
- [ ] 5.5 Test: value provider, factory provider, alias, config access

## Phase 6: Full Example + Documentation (6 items)

### Example: complete REST API (CMMV-style)
- [ ] 6.1 `samples/api-cmmv/` — contract-driven CRUD API
- [ ] 6.2 TaskContract → auto-generated TaskController + TaskService
- [ ] 6.3 UserContract with auth → AuthGuard integration
- [ ] 6.4 AppModule composing TaskModule + UserModule
- [ ] 6.5 Integration with std::db (SQLite) for persistence
- [ ] 6.6 Documentation: "Building APIs in TML — CMMV-style" guide

## Architecture: CMMV vs NestJS vs TML

| Feature | NestJS | CMMV | **TML** |
|---------|--------|------|---------|
| DI Container | Heavy IoC + reflect-metadata | Singleton registry | **Singleton registry** (CMMV) |
| Injection | Constructor (auto) | Constructor (manual) | **Field (direct ref)** |
| Module | Recursive tree | Flat list | **Flat + submodule import** (CMMV) |
| Services | Scoped (singleton/request/transient) | Singleton only | **Singleton default** (CMMV) |
| Contracts | ❌ | ✅ Auto-generate CRUD | **✅ Auto-generate CRUD** (CMMV) |
| Startup cost | ~15ms (DI resolution) | ~1ms (flat registry) | **~0ms (compile-time wiring)** |
| Reflection | Runtime reflect-metadata | None | **None (compile-time decorators)** |
| Custom providers | useClass/useValue/useFactory | Value + factory | **Value + factory + alias** |

### Why This is Better Than NestJS
1. **Zero DI overhead** — no proxy objects, no scope resolution, no reflection
2. **Compile-time safety** — decorator errors caught at compile time, not runtime
3. **Contract-driven** — CMMV's best feature: define entity → get full CRUD API
4. **Explicit is better** — you SEE the wiring in Module::new().service(...), no hidden magic
5. **Same DX** — decorators look the same: @Controller, @Get, @Service

### Dependency on phase0_nestjs-http-decorators
- Phase 3 (HTTP Integration) requires @Controller from the HTTP decorators task
- Phases 1-2 are independent and can be implemented first
- The DI system is general-purpose — not HTTP-specific
