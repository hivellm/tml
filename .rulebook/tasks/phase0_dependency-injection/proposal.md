# Proposal: Dependency Injection — CMMV-Inspired + NestJS-Complete

## Why
Without DI, controllers can't use services, services can't use repositories, and the
entire layered architecture collapses. This is the missing piece that makes the HTTP
framework usable for real applications.

## CMMV vs NestJS — Why CMMV's Approach is Better for TML

### NestJS: Heavy IoC Container
- Runtime reflection via `reflect-metadata` (slow, large dependency)
- Complex container with proxy objects, scope resolution, circular dependency detection
- Constructor injection requires TypeScript's `emitDecoratorMetadata` compiler flag
- Overhead: ~15ms startup penalty for DI resolution in medium apps

### CMMV: Lightweight Singleton + Registry
- **No dependency context control** — services are singletons by default, no IoC container
- Services registered in a `ServiceRegistry` (global flat map)
- Constructor params are just regular params — no automatic injection magic
- **Contract-driven generation** — controllers/services auto-generated from contracts
- Shared services use singleton pattern for global access without repeated instantiation
- Result: **4x faster startup, near-zero DI overhead**

### TML Approach: CMMV-Style Simplicity + NestJS Decorators
Combine CMMV's performance (singleton registry, no heavy IoC) with NestJS's DX (decorators):

```tml
@Service("user")
type UserService {
    db: SqliteConnection
}

impl UserService {
    pub func new(db: SqliteConnection) -> UserService {
        return UserService { db: db }
    }

    pub func find_all(this) -> Str { ... }
}

@Controller("/users")
type UserController {
    service: ref UserService   // direct reference, no proxy
}

// Module wires everything at startup — flat, no recursion
let module = Module::new()
    .provider(UserService::new(db))
    .controller(UserController { service: ref user_svc })
    .build()

app.bootstrap(module)
```

**Key decisions:**
1. **Singleton services** — one instance, registered in flat registry (CMMV style)
2. **Explicit wiring in Module** — no automatic injection, no reflection (CMMV style)
3. **@Service/@Controller decorators** — metadata for organization + codegen (NestJS style)
4. **No IoC container** — direct references, zero overhead (CMMV style)
5. **Contract-driven option** — can auto-generate controllers/services from contracts (CMMV style)

## Impact
- Affected code: lib/std/src/di/ (new module), lib/std/src/http/ (integration)
- Breaking change: NO (additive)
- User benefit: NestJS DX with CMMV performance — zero-overhead DI
