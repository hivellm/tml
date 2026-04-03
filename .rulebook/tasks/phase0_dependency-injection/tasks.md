# Tasks: Dependency Injection — NestJS-Complete IoC Container

**Status**: Planning. 0% (0/48).
**Target API**:
```tml
@Module(controllers=[UserController], providers=[UserService, DbProvider])
type AppModule {}

@Injectable
type UserService {
    @Inject
    db: ref SqliteConnection
}

@Controller("/users")
type UserController {
    @Inject
    userService: ref UserService
}
```

## How NestJS DI Works (Reference)

```
1. @Module declares providers[] and controllers[]
2. Container scans @Injectable types for @Inject fields
3. Topological sort: if A depends on B, create B first
4. Singleton scope: one instance per container (default)
5. Request scope: new instance per HTTP request
6. Transient scope: new instance per injection point
7. Custom providers: { provide: TOKEN, useClass/useValue/useFactory }
```

## Phase 1: Core Container — Provider Registry + Resolution (10 items)

### Provider types
- [ ] 1.1 `di/provider.tml` — `ProviderKind` enum: Class, Value, Factory
- [ ] 1.2 `di/provider.tml` — `Provider` type: token (Str), kind, instance_ptr (I64), is_resolved (Bool)
- [ ] 1.3 `di/provider.tml` — `Provider::class_provider(token)` — creates from @Injectable type name
- [ ] 1.4 `di/provider.tml` — `Provider::value_provider(token, value_ptr)` — pre-built value (like useValue)
- [ ] 1.5 `di/provider.tml` — `Provider::factory_provider(token, factory_fn)` — lazy via factory (like useFactory)

### Container
- [ ] 1.6 `di/container.tml` — `Container` type: providers registry (flat array), resolved instances
- [ ] 1.7 `Container::register(provider)` — adds a provider to the registry
- [ ] 1.8 `Container::resolve(token) -> I64` — returns instance pointer, creates if not yet resolved
- [ ] 1.9 `Container::resolve_all()` — resolves all registered providers (startup phase)
- [ ] 1.10 `Container::get[T](token) -> ref T` — typed accessor (returns ref to resolved instance)

## Phase 2: @Injectable + @Inject Decorators (8 items)

### Compiler: decorator recognition
- [ ] 2.1 Parser: `@Injectable` on type declarations → marks type as DI provider
- [ ] 2.2 Parser: `@Inject` on struct fields → marks field for injection (already supported via field decorators)
- [ ] 2.3 Codegen: collect all @Injectable types → generate provider metadata table
- [ ] 2.4 Codegen: for each @Inject field, generate injection code (set field ptr from container)

### Library: injection helpers
- [ ] 2.5 `di/injectable.tml` — `InjectableMetadata` type: type_name, dependencies (field names + tokens)
- [ ] 2.6 `di/inject.tml` — `inject_field(instance_ptr, field_offset, dependency_ptr)` — raw ptr write
- [ ] 2.7 `di/scan.tml` — `scan_injectables()` → collects @Injectable metadata at startup

### Tests
- [ ] 2.8 Test: @Injectable type + @Inject field → container resolves and injects

## Phase 3: @Module — Module System (8 items)

### Module definition
- [ ] 3.1 `di/module.tml` — `ModuleDef` type: name, providers list, controllers list, imports list, exports list
- [ ] 3.2 `@Module(providers=[...], controllers=[...])` decorator on types
- [ ] 3.3 `ModuleDef::providers(token_list)` — register providers from module declaration
- [ ] 3.4 `ModuleDef::controllers(token_list)` — register controllers
- [ ] 3.5 `ModuleDef::imports(module_list)` — import providers from other modules

### Module container
- [ ] 3.6 `di/module_container.tml` — `ModuleContainer` wraps Container + module tree resolution
- [ ] 3.7 `ModuleContainer::bootstrap(root_module)` — resolves entire dependency graph from root
- [ ] 3.8 Circular dependency detection — error if A → B → A

## Phase 4: Dependency Graph Resolution (6 items)

### Topological sort
- [ ] 4.1 `di/graph.tml` — `DependencyGraph` type: adjacency list (token → [dependency tokens])
- [ ] 4.2 `build_graph(providers)` — scans @Inject fields to build adjacency list
- [ ] 4.3 `topological_sort(graph) -> Outcome[List[Str], Str]` — Kahn's algorithm
- [ ] 4.4 Circular dependency detection in topological_sort → returns error with cycle path

### Resolution order
- [ ] 4.5 `resolve_in_order(container, sorted_tokens)` — creates instances in dependency order
- [ ] 4.6 Tests: A → B → C resolves in order C, B, A; circular A → B → A detected

## Phase 5: Provider Scopes (6 items)

### Scope types
- [ ] 5.1 `di/scope.tml` — `Scope` enum: Singleton, Request, Transient
- [ ] 5.2 `Provider::with_scope(scope)` — sets the lifecycle scope
- [ ] 5.3 Singleton: one instance per container (default, created at startup)
- [ ] 5.4 Request: new instance per HTTP request (created in middleware, destroyed after response)
- [ ] 5.5 Transient: new instance per injection point (created each time resolve() is called)

### Tests
- [ ] 5.6 Test: singleton returns same instance, transient returns different instances

## Phase 6: Custom Providers — useValue, useFactory (5 items)

### Advanced provider patterns
- [ ] 6.1 `Provider::use_value(token, value)` — static value injection (config, constants)
- [ ] 6.2 `Provider::use_factory(token, factory_fn)` — lazy construction with factory function
- [ ] 6.3 `Provider::use_existing(token, existing_token)` — alias (inject B when A is requested)
- [ ] 6.4 String-based tokens for dynamic providers: `Container::register_named("DB_CONNECTION", ptr)`

### Tests
- [ ] 6.5 Test: useValue injects config, useFactory creates with deps, useExisting aliases

## Phase 7: HTTP Integration — Controllers + Services (5 items)

### Wiring DI to HTTP framework
- [ ] 7.1 `App::bootstrap(module)` — creates container from @Module, resolves all, registers controllers
- [ ] 7.2 Controller methods auto-receive injected services via `this.service` fields
- [ ] 7.3 Request-scoped providers: create per-request container with request-scoped deps
- [ ] 7.4 `http/context.tml` — `RequestContext` type carrying request-scoped providers

### Tests
- [ ] 7.5 Test: full flow — App.bootstrap(AppModule) → UserController.findAll() uses UserService

## Phase 8: Full Example + Documentation (5 items)

### Example: layered API
- [ ] 8.1 `samples/api-di/` — complete app with Module → Controller → Service → Repository → DB
- [ ] 8.2 UserModule: UserController + UserService + UserRepository
- [ ] 8.3 AuthModule: AuthGuard + AuthService + JwtProvider
- [ ] 8.4 AppModule imports UserModule + AuthModule
- [ ] 8.5 Documentation: "Dependency Injection in TML" guide

## Architecture Notes

### How It Works Without Runtime Reflection

NestJS uses TypeScript decorators + reflect-metadata for runtime reflection.
TML doesn't have runtime reflection. Instead:

1. **Compile-time metadata generation** — the codegen collects @Injectable/@Inject
   decorators and generates a static metadata table:
   ```llvm
   ; Generated by compiler for @Injectable UserService with @Inject db field
   @__tml_di_meta_UserService = private constant { ptr, i32, ... }
   ```

2. **Startup resolution** — Container reads the metadata table and creates instances
   in topological order, writing pointers to @Inject fields.

3. **No constructor injection** — TML uses field injection (@Inject on fields)
   because TML types don't have constructors in the NestJS sense. Equivalent to
   NestJS property injection with `@Inject()`.

### NestJS ↔ TML Mapping

| NestJS | TML | Notes |
|--------|-----|-------|
| `@Injectable()` | `@Injectable` | Marks type as DI provider |
| `@Inject(TOKEN)` | `@Inject` on field | Field-level injection |
| `constructor(private svc: Service)` | `@Inject svc: ref Service` | Field injection instead of ctor |
| `@Module({ providers, controllers })` | `@Module(providers=[...])` | Module declaration |
| `useValue` | `Provider::use_value()` | Static value provider |
| `useFactory` | `Provider::use_factory()` | Factory provider |
| `useClass` | `Provider::class_provider()` | Default — type itself |
| `Scope.DEFAULT` | `Scope::Singleton` | One instance (default) |
| `Scope.REQUEST` | `Scope::Request` | Per HTTP request |
| `Scope.TRANSIENT` | `Scope::Transient` | Per injection point |

### Dependency on phase0_nestjs-http-decorators
- Phase 7 (HTTP Integration) requires @Controller from the HTTP decorators task
- Phases 1-6 are independent and can be implemented first
- The DI system is general-purpose — not HTTP-specific
