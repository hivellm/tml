# Tasks: Dependency Injection — CMMV-Style Application Bootstrap

**Status**: Planning. 0% (0/31).
**Approach**: CMMV Application.create() pattern — one declarative call, zero boilerplate
**Target API**:

### User-Facing Code (what developers write)

```tml
// === Services — @Service decorator, singleton ===

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

@Service("auth")
type AuthService {}

impl AuthService {
    pub func validate_token(this, token: Str) -> Bool {
        return token == "secret-token"
    }
}

// === Controllers — use services via fields ===

@Controller("/users")
type UserController {
    svc: ref UserService
}

// === Modules — CMMV-style flat composition ===

let UserModule = Module::new("users")
    .service(user_svc)
    .controller(UserController { svc: ref user_svc })

let AuthModule = Module::new("auth")
    .service(AuthService {})

let AppModule = Module::new("app")
    .import(UserModule)
    .import(AuthModule)

// === Bootstrap — one call, like CMMV Application.create() ===

func main() -> I32 {
    let db = SqliteConnection::open_in_memory().unwrap()
    let user_svc = UserService::new(db)

    Application::create({
        http_adapter: "default",
        modules: [AppModule],
        config: {
            port: 3000,
            cors: "*"
        }
    })
    return 0
}
```

### What `Application::create()` Does Internally
```
1. For each module: register services in ServiceRegistry (singleton)
2. For each controller: register routes in radix-tree router
3. Start HTTP server on config.port
4. Block on accept loop
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

## Phase 2: Module — CMMV-style Declarative Composition (8 items)

### Module type (mirrors CMMV's `new Module({...})`)
- [ ] 2.1 `di/module.tml` — `Module` type: name, controllers (Str list), providers (Str list), submodules (Str list), contracts (Str list)
- [ ] 2.2 `Module::new(name)` → returns empty module with name
- [ ] 2.3 `.controller(ctrl_ptr)` → adds controller to module
- [ ] 2.4 `.service(svc_instance)` → adds service as singleton provider
- [ ] 2.5 `.import(sub_module)` → imports another module's providers (CMMV submodules)

### @Service decorator
- [ ] 2.7 `@Service("token")` on types → compile-time metadata, registers in ServiceRegistry at bootstrap

### Tests
- [ ] 2.8 Test: Module::new("app").service(svc).import(sub_mod) composes correctly

## Phase 3: Application::create() — CMMV Bootstrap (8 items)

### Application entry point (mirrors CMMV `Application.create({...})`)
- [ ] 3.1 `di/application.tml` — `AppConfig` type: http_adapter (Str), port (I32), db_path (Str), cors (Str)
- [ ] 3.2 `Application::create(config, modules)` — single declarative bootstrap call
- [ ] 3.3 Internally: resolve all module providers → register in ServiceRegistry
- [ ] 3.6 Internally: collect all controllers → register routes in App router
- [ ] 3.7 Internally: start HTTP server on config.port

### Tests
- [ ] 3.8 Test: Application::create() with one module + one contract → server starts, routes work

## Phase 4: Custom Providers — useValue, useFactory (5 items)

### Advanced patterns (same as NestJS but simpler)
- [ ] 5.1 `ServiceRegistry::register_value(token, i64_value)` — raw value (config, port number)
- [ ] 5.2 `ServiceRegistry::register_factory(token, factory_fn)` — lazy, called on first get()
- [ ] 5.3 `ServiceRegistry::alias(new_token, existing_token)` — alias lookup
- [ ] 5.4 Config provider: `register_config(key, value)` + `get_config(key)` — typed config access

### Tests
- [ ] 5.5 Test: value provider, factory provider, alias, config access

## Phase 5: Full Example + Documentation (5 items)

### Example: REST API with DI
- [ ] 5.1 `samples/api-di/` — Module + Controller + Service + DB
- [ ] 5.2 UserController + UserService + SqliteConnection wiring
- [ ] 5.3 AuthModule with AuthService
- [ ] 5.4 AppModule composing UserModule + AuthModule
- [ ] 5.5 Documentation: "Dependency Injection in TML" guide

## Architecture: CMMV vs NestJS vs TML

| Feature | NestJS | CMMV | **TML** |
|---------|--------|------|---------|
| DI Container | Heavy IoC + reflect-metadata | Singleton registry | **Singleton registry** (CMMV) |
| Injection | Constructor (auto) | Constructor (manual) | **Field (direct ref)** |
| Module | Recursive tree | Flat list | **Flat + submodule import** (CMMV) |
| Services | Scoped (singleton/request/transient) | Singleton only | **Singleton default** (CMMV) |
| Startup cost | ~15ms (DI resolution) | ~1ms (flat registry) | **~0ms (compile-time wiring)** |
| Reflection | Runtime reflect-metadata | None | **None (compile-time decorators)** |
| Custom providers | useClass/useValue/useFactory | Value + factory | **Value + factory + alias** |

### Why This is Better Than NestJS
1. **Zero DI overhead** — no proxy objects, no scope resolution, no reflection
2. **Compile-time safety** — decorator errors caught at compile time, not runtime
3. **Explicit is better** — you SEE the wiring in Module::new().service(...), no hidden magic
4. **Same DX** — decorators look the same: @Controller, @Get, @Service

### Dependency on phase0_nestjs-http-decorators
- Phase 3 (HTTP Integration) requires @Controller from the HTTP decorators task
- Phases 1-2 are independent and can be implemented first
- The DI system is general-purpose — not HTTP-specific
