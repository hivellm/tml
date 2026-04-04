# Dependency Injection in TML — Zero-Copy Pointer DI

TML's DI system is built on raw pointers. There is no IoC container, no reflection, no proxy layer. Services are plain types. Wiring is explicit pointer assignment. Startup overhead is zero.

## Why Pointers Instead of a Container

| | NestJS | CMMV | TML |
|--|--------|------|-----|
| Mechanism | IoC + reflect-metadata + proxy | Singleton registry + manual ctor | Raw pointer assignment |
| Startup cost | ~15ms | ~1ms | ~0ns |
| Runtime overhead per call | ~50ns (proxy deref + map lookup) | ~10ns (registry lookup) | ~1ns (single MOV instruction) |
| Memory | Instance + proxy + metadata | Instance + registry entry | Instance + 8-byte pointer |

## Core Modules

```
use std::di::registry::ServiceRegistry   // HashMap[Str, I64] — token → raw pointer
use std::di::module::Module              // groups providers and controllers
use std::di::application::AppConfig      // port + CORS config
use std::di::application::bootstrap_modules  // counts services, wires topology
use std::di::config::InjectedConfig      // key-value config store
```

## Service Registry

`ServiceRegistry` stores services as raw pointers (I64) keyed by a string token.

```tml
use std::di::registry::ServiceRegistry

var reg = ServiceRegistry::new()
reg.provide("user_svc", &my_service as I64)

let ptr: Maybe[I64] = reg.resolve("user_svc")
if ptr.is_just() {
    // use the pointer
}

let count: I64 = reg.len()
let exists: Bool = reg.has("user_svc")
```

## Module

A `Module` groups service providers and controller registrations. Built with a fluent API.

```tml
use std::di::module::Module

let m = Module::new("app")
    .provide("db",   &db_svc as I64)
    .provide("user", &user_svc as I64)
    .controller("/users", &ctrl as I64)

let count: I64 = m.service_count()     // 2
let ctrls: I32 = m.controller_count()  // 1
let ptr:   Maybe[I64] = m.resolve("db")
```

## Application Bootstrap

`bootstrap_modules` accepts a config and a module, counts topology, and returns an `ApplicationInfo`.

```tml
use std::di::application::{AppConfig, bootstrap_modules}

let config = AppConfig::new(3000).with_cors("*")
let info = bootstrap_modules(config, my_module)
println(info.summary())
// => "Application: 2 services, 1 controllers, port 3000"
```

## Config

`InjectedConfig` is a key-value string store. Integer and boolean accessors handle conversion.

```tml
use std::di::config::InjectedConfig

var cfg = InjectedConfig::new()
cfg.set("port",  "3000")
cfg.set("debug", "true")
cfg.set("cors",  "*")

let port: I64 = cfg.get_i64_or("port", 8080)       // 3000
let cors: Str  = cfg.get_or("cors", "")             // "*"
let dbg:  Maybe[Bool] = cfg.get_bool("debug")       // Just(true)
let has:  Bool = cfg.has("port")                    // true
```

## Full Example

The sample in `samples/api-di/main.tml` demonstrates the full pattern: services on the stack, pointers in the module, bootstrap to get topology, then direct service calls.

```tml
use std::di::registry::ServiceRegistry
use std::di::module::Module
use std::di::application::{AppConfig, bootstrap_modules}
use std::di::config::InjectedConfig

// --- Services ---

type DbService { conn_ptr: I64 }
impl DbService {
    pub func new(conn_ptr: I64) -> DbService {
        return DbService { conn_ptr: conn_ptr }
    }
    pub func execute(this, sql: Str) -> Str { return "ok" }
}

type UserService { db_ptr: I64 }
impl UserService {
    pub func new(db_ptr: I64) -> UserService {
        return UserService { db_ptr: db_ptr }
    }
    pub func find_all(this) -> Str {
        return "[{\"id\":1,\"name\":\"alice\"}]"
    }
}

// --- Bootstrap ---

func main() -> I32 {
    var db_svc   = DbService::new(0)
    var user_svc = UserService::new(&db_svc as I64)

    var cfg = InjectedConfig::new()
    cfg.set("port", "3000")

    let app_module = Module::new("app")
        .provide("db",   &db_svc   as I64)
        .provide("user", &user_svc as I64)

    let app_config = AppConfig::new(cfg.get_i64_or("port", 3000) as I32)
    let info = bootstrap_modules(app_config, app_module)

    println(info.summary())
    println(user_svc.find_all())
    return 0
}
```

Running this with `tml run samples/api-di/main.tml` prints:
```
Application: 2 services, 0 controllers, port 3000
[{"id":1,"name":"alice"}]
```

## What's Not Yet Available

The following require compiler changes and are tracked in `phase0_dependency-injection`:

- `@Service` decorator on types (compile-time metadata, Phase 2.7)
- `@Controller("/prefix")` on types with automatic route collection (Phase 1 of `phase0_nestjs-http-decorators`)
- `@Inject()` constructor injection (requires compile-time DI resolution)
- Automatic constructor parameter wiring

These features require parser and HIR support for decorator-on-type and decorator-on-param nodes. The wiring shown above (explicit pointer passing) is the current idiomatic approach and has zero runtime overhead.
