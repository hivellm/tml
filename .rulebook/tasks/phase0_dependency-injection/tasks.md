# Tasks: Dependency Injection — Pointer-Based, Zero-Copy, CMMV-Style

**Status**: Planning. 0% (0/30).
**Approach**: Native pointers for DI (TML advantage over TS/JS), CMMV bootstrap pattern
**Key insight**: TML has real pointers — services are linked via `*Type` references,
zero copy, zero proxy, zero serialization. What NestJS needs reflect-metadata for,
TML does with a pointer write.

### Target API (what developers write)

```tml
use std::http::app::Application
use std::di::{Module, Service}
use std::db::sqlite::connection::SqliteConnection

// === Services — regular types, @Service is just metadata ===

@Service
type UserService {
    db: *SqliteConnection    // pointer to shared DB connection
}

impl UserService {
    pub func new(db: *SqliteConnection) -> UserService {
        return UserService { db: db }
    }
    pub func find_all(this) -> Str { return "[{\"id\":1}]" }
}

@Service
type AuthService {}

impl AuthService {
    pub func validate(this, token: Str) -> Bool {
        return token != ""
    }
}

// === Controller — fields are pointers to services ===

@Controller("/users")
type UserController {
    user_svc: *UserService,
    auth_svc: *AuthService
}

impl UserController {
    @Get("/")
    func list(this, req: IncomingMessage) -> Str {
        return this.user_svc.find_all()
    }

    @Get("/:id")
    func get_one(this, req: IncomingMessage) -> Str {
        return "{\"id\":1}"
    }
}

// === Bootstrap — services created, pointers wired, server starts ===

func main() -> I32 {
    var db = SqliteConnection::open_in_memory().unwrap()
    var user_svc = UserService::new(&db)
    var auth_svc = AuthService {}

    Application::create({
        port: 3000,
        modules: [
            Module::new("app")
                .provide(&user_svc)            // registers pointer in ServiceRegistry
                .provide(&auth_svc)
                .controller(&UserController {   // controller gets pointers directly
                    user_svc: &user_svc,
                    auth_svc: &auth_svc
                })
        ]
    })
    return 0
}
```

### What Happens Under the Hood
```
1. user_svc lives on main's stack (or heap via Heap::new)
2. &user_svc = raw pointer to the service (8 bytes)
3. UserController.user_svc = same pointer (8 bytes, no copy)
4. When handler runs: this.user_svc.find_all() = pointer deref + vtable call
5. ServiceRegistry stores {token → ptr} for global access
6. Total DI overhead: 0 ns (just pointer assignment at startup)
```

### Why This is Better Than CMMV/NestJS
```
NestJS:  constructor injection → reflect-metadata → proxy → ~15ms startup
CMMV:    singleton registry → manual constructor call → ~1ms startup
TML:     pointer assignment → zero-copy field write → ~0ns overhead
```

## Phase 1: Service Registry — Pointer Store (6 items)

### Core registry (stores raw pointers to service instances)
- [ ] 1.1 `di/registry.tml` — `ServiceRegistry` type: flat HashMap[Str, I64] mapping token → raw pointer (I64)
- [ ] 1.2 `ServiceRegistry::new()`, `register(token, ptr: I64)`, `get(token) -> Maybe[I64]`
- [ ] 1.3 `ServiceRegistry::has(token) -> Bool`, `list() -> Str`
- [ ] 1.4 Global convenience: `provide(name, ptr)`, `resolve(name) -> I64`

### Tests
- [ ] 1.5 Test: register + get returns same pointer value
- [ ] 1.6 Test: get nonexistent returns Nothing

## Phase 2: Module — Pointer-Based Composition (7 items)

### Module builder (passes pointers, not instances)
- [ ] 2.1 `di/module.tml` — `Module` type with name + provider_count + controller_count
- [ ] 2.2 `Module::new(name)` → empty module
- [ ] 2.3 `.provide(ptr: I64)` → registers a service pointer (stored in ServiceRegistry)
- [ ] 2.4 `.controller(ptr: I64, prefix: Str)` → registers a controller pointer + route prefix
- [ ] 2.5 `.import(other: Module)` → merges another module's services (CMMV submodules)

### @Service decorator
- [ ] 2.6 `@Service` on types → compile-time metadata (token = type name)

### Tests
- [ ] 2.7 Test: Module with 2 providers + 1 controller builds correctly

## Phase 3: Application::create() — Pointer Wiring + Server Start (6 items)

### Bootstrap (wires pointers, starts server)
- [ ] 3.1 `di/application.tml` — `AppConfig` type: port (I32), cors (Str)
- [ ] 3.2 `Application::create(config, modules)` — iterates modules, registers all in ServiceRegistry
- [ ] 3.3 Internally: for each controller → register @Get/@Post routes in App router
- [ ] 3.4 Internally: start HTTP server on config.port, block on accept loop
- [ ] 3.5 `Application::create()` is the ONLY entry point — no manual App::new() needed

### Tests
- [ ] 3.6 Test: Application::create() with module → routes registered (unit test, no actual server)

## Phase 4: Config + Value Providers (5 items)

### Config and static values (no factory — keep it simple)
- [ ] 4.1 `di/config.tml` — `Config` type: flat HashMap[Str, Str] for key-value settings
- [ ] 4.2 `Config::new()`, `set(key, value)`, `get(key) -> Maybe[Str]`, `get_i64(key) -> Maybe[I64]`
- [ ] 4.3 `ServiceRegistry::register_config(config_ptr)` — stores Config as a special provider
- [ ] 4.4 `ServiceRegistry::alias(new_token, existing_token)` — alias lookup

### Tests
- [ ] 4.5 Test: Config set/get, alias resolve, config provider in registry

## Phase 5: Full Example + Documentation (6 items)

### Example: REST API with pointer-based DI
- [ ] 5.1 `samples/api-di/main.tml` — complete app: DB → Service → Controller → routes
- [ ] 5.2 `samples/api-di/user_service.tml` — @Service with *SqliteConnection field
- [ ] 5.3 `samples/api-di/user_controller.tml` — @Controller with *UserService field
- [ ] 5.4 `samples/api-di/app_module.tml` — Module::new().provide().controller()
- [ ] 5.5 Verify: `tml run samples/api-di/main.tml` starts server on port 3000
- [ ] 5.6 Documentation: "Dependency Injection in TML — Zero-Copy Pointer DI" guide

## Architecture Comparison

| | NestJS | CMMV | **TML** |
|--|--------|------|---------|
| **Mechanism** | IoC + reflect-metadata + proxy | Singleton registry + manual ctor | **Raw pointer assignment** |
| **Service storage** | Container Map<Token, Instance> | ServiceRegistry HashMap | **ServiceRegistry HashMap[Str, I64]** (ptr) |
| **Injection** | Constructor param (auto-resolved) | Constructor param (manual) | **Field pointer write (zero-copy)** |
| **Controller wiring** | `@Inject()` + DI resolve | Manual in Module | **`&service` pointer in struct literal** |
| **Startup cost** | ~15ms | ~1ms | **~0ns (pointer assignment)** |
| **Runtime overhead** | Proxy deref per call | None | **None (direct pointer deref)** |
| **Memory** | Instance + proxy + metadata | Instance + registry entry | **Instance + 8-byte ptr** |

### Why Pointers > Everything Else
```
// NestJS (JS): service access = proxy lookup + map get + type check
this.userService.findAll()  // ~50ns overhead per call

// CMMV (TS): service access = registry lookup + type assertion
this.userService.findAll()  // ~10ns overhead per call

// TML: service access = pointer dereference (single CPU instruction)
this.user_svc.find_all()    // ~1ns overhead (MOV instruction)
```

### Dependency on phase0_nestjs-http-decorators
- Phase 3 requires @Controller + @Get from the HTTP decorators task
- Phases 1-2 are independent (pure DI, no HTTP)
- The DI system is general-purpose — works for any service wiring
